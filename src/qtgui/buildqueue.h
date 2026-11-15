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
#ifndef BTQT_BUILDQUEUE_H
#define BTQT_BUILDQUEUE_H

#include "rowsmodel.h"

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QPointer>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <deque>
#include <functional>

/* Work that can wait (App.builds): what the user has not asked to see yet
 * -- the Puzzle workspace's cards before Puzzle first shows, the view of the
 * pieces not chosen, the rows of a long list past its first screen. Each
 * owner has at most one job of one or more steps; the steps run one at a
 * time on the GUI thread, a gap apart, so frames and input go on between
 * them and the user never meets one long stall.
 *
 * Nothing that is not urgent runs before startAfter() (Main.qml, a second
 * after the first frame with the workspace) -- start-up is not slowed by
 * it. An urgent job (its view is on screen) goes before the others and is
 * not held back by that delay.
 *
 * A step answers Done, More (run me again, before the jobs behind me) or
 * Waiting: it started something that finishes by itself, an asynchronous
 * Loader; the jobs that are not urgent wait for its resume().
 */
class BuildQueue : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("App.builds")

  // nothing queued, nothing being waited for
  Q_PROPERTY(bool idle READ idle NOTIFY idleChanged FINAL)
  // the jobs that are not urgent are held: before startAfter(), and during its delay
  Q_PROPERTY(bool holding READ holding NOTIFY holdingChanged FINAL)
  // the free time between two steps, in ms
  Q_PROPERTY(int gap READ gap WRITE setGap NOTIFY gapChanged FINAL)

public:

  enum class Step { Done, More, Waiting };
  using Work = std::function<Step()>;

  // what one step aims to take, for those that size their own (BatchedRows)
  static constexpr qint64 stepBudgetNs = 6'000'000;

  explicit BuildQueue(QObject * parent = nullptr);
  ~BuildQueue() override;

  /* Queues owner's job, or replaces the work of the one it has; posting an
   * urgent job raises a queued one. The job goes with its owner. */
  void post(QObject * owner, Work work, bool urgent);
  void cancel(QObject * owner);
  bool queued(const QObject * owner) const;
  // owner's step that answered Waiting is over
  void resume(QObject * owner);

  /* Lets the jobs that are not urgent run ms from now (again: a new window
   * holds them for its own start). */
  Q_INVOKABLE void startAfter(int ms);

  bool idle(void) const { return m_jobs.empty() && m_waiting.isNull(); }
  bool holding(void) const { return m_holding; }
  int gap(void) const { return m_gap; }
  void setGap(int ms);

  // the steps run so far, and how long the last took (tests; the start-up trace)
  int stepsRun(void) const { return m_stepsRun; }
  qint64 lastStepNs(void) const { return m_lastStepNs; }

signals:

  void idleChanged(void);
  void holdingChanged(void);
  void gapChanged(void);
  /* after each step (lastStepNs). No arguments: a QObject * or qint64 in the
   * module's types makes qmltyperegistrar declare QObject an opaque value type,
   * and every QObject type, App first, stops compiling ahead of time. */
  void stepped(void);

private:

  struct Job {
    QPointer<QObject> owner;
    Work work;
    bool urgent;
    QMetaObject::Connection gone;
  };

  void schedule(void);
  void runNext(void);
  void setHolding(bool holding);
  void noteIdle(bool wasIdle);
  std::deque<Job>::iterator find(const QObject * owner);
  void insert(Job job);

  std::deque<Job> m_jobs;               // the urgent first, each kind in order posted
  QPointer<QObject> m_waiting;
  QMetaObject::Connection m_waitGone;
  QPointer<QObject> m_running;
  bool m_runningCancelled = false;
  bool m_holding = true;
  int m_gap = 30;
  int m_stepsRun = 0;
  qint64 m_lastStepNs = 0;
  QTimer m_timer;
  QTimer m_hold;
  QElapsedTimer m_sinceStep;
};

/* A piece of the window built when the queue gets to it (BuildJob in QML):
 * `granted` turns true then, or at once when `needed` is (the user is
 * looking), and stays true. With `waits`, the queue holds the jobs behind
 * it until finish() -- a Loader with `asynchronous: true` is made over
 * several frames by Qt Quick's incubator, a few ms in each:
 *
 *   BuildJob { id: job; queue: App.builds; waits: true; needed: shown }
 *   Loader { active: job.granted; asynchronous: !job.needed
 *            onStatusChanged: if (status !== Loader.Loading) job.finish() }
 *
 * (a Loader turned synchronous while loading finishes at once). Needed at
 * once, it is granted when made; else it is queued a turn after.
 */
class BuildJob : public QObject {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(BuildQueue * queue READ queue WRITE setQueue NOTIFY queueChanged FINAL)
  Q_PROPERTY(bool wanted READ wanted WRITE setWanted NOTIFY wantedChanged FINAL)
  Q_PROPERTY(bool needed READ needed WRITE setNeeded NOTIFY neededChanged FINAL)
  Q_PROPERTY(bool waits READ waits WRITE setWaits NOTIFY waitsChanged FINAL)
  Q_PROPERTY(bool granted READ granted NOTIFY grantedChanged FINAL)

public:

  explicit BuildJob(QObject * parent = nullptr);
  ~BuildJob() override;

  BuildQueue * queue(void) const { return m_queue; }
  void setQueue(BuildQueue * queue);
  bool wanted(void) const { return m_wanted; }
  void setWanted(bool wanted);
  bool needed(void) const { return m_needed; }
  void setNeeded(bool needed);
  bool waits(void) const { return m_waits; }
  void setWaits(bool waits);
  bool granted(void) const { return m_granted; }

  // what the grant started is done
  Q_INVOKABLE void finish(void);

  /* Its properties are set: it may be queued now. Called a turn after it is
   * made (QQmlParserStatus would do it at once, but qmlcachegen cannot see
   * that interface's type, and leaves the object unresolved); tests call it. */
  void componentComplete(void);

signals:

  void queueChanged(void);
  void wantedChanged(void);
  void neededChanged(void);
  void waitsChanged(void);
  void grantedChanged(void);

private:

  void update(void);
  void grant(void);

  QPointer<BuildQueue> m_queue;
  bool m_wanted = true;
  bool m_needed = false;
  bool m_waits = false;
  bool m_granted = false;
  bool m_finished = false;
  bool m_complete = false;
};

/* A list model's rows handed to a view a batch at a time (Repeater, Flow,
 * ListView): the view sees the source's first `count` rows, and the queue
 * brings in the rest, each step sized to take about stepBudgetNs. A shown
 * view gets its first screen (`immediate` rows) at once and the rest as
 * urgent steps; a hidden one is built when the queue gets to it.
 *
 * Once it has been whole, it keeps up with its source as it changes: a
 * few rows added or taken away -- a piece added or removed -- reach the view
 * at once, as the source's own inserts and removes, so the view makes or
 * drops only those delegates. Many at once (a new puzzle) come in batches
 * again. Inactive, it hands over no rows; without a queue, all of them, a
 * turn after it is set up (QML may set the source before the queue).
 */
class BatchedRows : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT

  // a RowsModel: QAbstractItemModel here would be an opaque type in the module (see stepped())
  Q_PROPERTY(RowsModel * source READ source WRITE setSource NOTIFY sourceChanged FINAL)
  Q_PROPERTY(BuildQueue * queue READ queue WRITE setQueue NOTIFY queueChanged FINAL)
  Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged FINAL)
  Q_PROPERTY(bool shown READ shown WRITE setShown NOTIFY shownChanged FINAL)
  Q_PROPERTY(int immediate READ immediate WRITE setImmediate NOTIFY immediateChanged FINAL)
  Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
  // every row of the source is in the view
  Q_PROPERTY(bool complete READ complete NOTIFY completeChanged FINAL)

public:

  explicit BatchedRows(QObject * parent = nullptr);
  ~BatchedRows() override;

  int rowCount(const QModelIndex & parent = QModelIndex()) const override;
  QVariant data(const QModelIndex & index, int role) const override;
  QHash<int, QByteArray> roleNames(void) const override;

  RowsModel * source(void) const { return m_source; }
  void setSource(RowsModel * source);
  BuildQueue * queue(void) const { return m_queue; }
  void setQueue(BuildQueue * queue);
  bool active(void) const { return m_active; }
  void setActive(bool active);
  bool shown(void) const { return m_shown; }
  void setShown(bool shown);
  int immediate(void) const { return m_immediate; }
  void setImmediate(int rows);
  int count(void) const { return m_count; }
  bool complete(void) const;

  // the rows the next step adds (tests)
  int batch(void) const { return m_batch; }

signals:

  void sourceChanged(void);
  void queueChanged(void);
  void activeChanged(void);
  void shownChanged(void);
  void immediateChanged(void);
  void countChanged(void);
  void completeChanged(void);

private:

  int sourceCount(void) const;
  int wantedCount(void) const;
  void reconcile(void);
  void grow(int rows);
  void shrinkTo(int rows);
  BuildQueue::Step step(void);
  void sync(int oldCount);

  QPointer<RowsModel> m_source;
  QPointer<BuildQueue> m_queue;
  QList<QMetaObject::Connection> m_connections;
  bool m_active = true;
  bool m_shown = false;
  bool m_whole = false;                // has been complete: keeps up at once from then on
  bool m_wasComplete = false;          // `complete` as last notified
  int m_immediate = 32;
  int m_count = 0;
  int m_batch = 8;
  // a source insert or remove within the rows handed over, between its two signals
  int m_pendingInsert = 0;
  int m_pendingRemove = 0;
  bool m_resetting = false;
  bool m_settling = false;             // no queue: all rows on the next turn
};

#endif
