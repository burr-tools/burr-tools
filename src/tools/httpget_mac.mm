/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "httpget.h"

#import <Foundation/Foundation.h>

/* NSURLSession is asynchronous; the caller is already a worker thread, so
 * the completion handler fills the result and a semaphore turns it back
 * into a blocking call. The session's own timeout guarantees the handler
 * runs. Built without ARC, hence the explicit release of the semaphore.
 */
HttpResult httpGet(const std::string & url, const std::string & userAgent, int timeoutSec) {

  HttpResult r;

  @autoreleasepool {

    NSURL * nsurl = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
    if (!nsurl) {
      r.error = "invalid URL";
      return r;
    }

    NSMutableURLRequest * req =
        [NSMutableURLRequest requestWithURL:nsurl
                                cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                            timeoutInterval:timeoutSec];
    [req setValue:[NSString stringWithUTF8String:userAgent.c_str()] forHTTPHeaderField:@"User-Agent"];
    [req setValue:@"application/vnd.github+json" forHTTPHeaderField:@"Accept"];

    NSURLSessionConfiguration * cfg = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    cfg.timeoutIntervalForRequest = timeoutSec;
    cfg.timeoutIntervalForResource = timeoutSec;
    NSURLSession * session = [NSURLSession sessionWithConfiguration:cfg];

    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    HttpResult * out = &r;

    NSURLSessionDataTask * task =
        [session dataTaskWithRequest:req
                   completionHandler:^(NSData * data, NSURLResponse * response, NSError * err) {
      if (err) {
        const char * msg = err.localizedDescription.UTF8String;
        out->kind = HttpResult::Kind::Transport;
        out->error = msg ? msg : "unknown network error";
      } else {
        if ([response isKindOfClass:[NSHTTPURLResponse class]])
          out->status = long([(NSHTTPURLResponse *)response statusCode]);

        if (data.length > HTTP_MAX_BODY) {
          out->kind = HttpResult::Kind::TooLarge;
          out->error = "response too large";
        } else if (out->status != 200) {
          out->kind = HttpResult::Kind::Status;
          out->error = "HTTP " + std::to_string(out->status);
        } else {
          out->kind = HttpResult::Kind::Ok;
          out->body.assign(static_cast<const char *>(data.bytes), data.length);
        }
      }
      dispatch_semaphore_signal(done);
    }];

    [task resume];
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
    [session finishTasksAndInvalidate];
    dispatch_release(done);
  }

  return r;
}
