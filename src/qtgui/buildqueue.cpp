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
#include "buildqueue.h"

#include <algorithm>
#include <utility>

// --- BuildQueue ----------------------------------------------------------------

BuildQueue::BuildQueue(QObject * parent) : QObject(parent) {
  m_timer.setSingleShot(true);
  // a few firings a second at most; a coarse one fires on Windows' ~15.6 ms tick, a gap early
  m_timer.setTimerType(Qt::PreciseTimer);
  connect(&m_timer, &QTimer::timeout, this, &BuildQueue::runNext);
  m_hold.setSingleShot(true);
  connect(&m_hold, &QTimer::timeout, this, [this] { setHolding(false); schedule(); });
  m_sinceStep.start();
}

BuildQueue::~BuildQueue() {
  for (Job & j : m_jobs)
    disconnect(j.gone);
  disconnect(m_waitGone);
}

std::deque<BuildQueue::Job>::iterator BuildQueue::find(const QObject * owner) {
  return std::find_if(m_jobs.begin(), m_jobs.end(), [owner](const Job & j) { return j.owner.data() == owner; });
}

bool BuildQueue::queued(const QObject * owner) const {
  return std::any_of(m_jobs.begin(), m_jobs.end(), [owner](const Job & j) { return j.owner.data() == owner; });
}

void BuildQueue::insert(Job job) {
  // after the urgent ones for an urgent job, at the end for the others
  auto at = job.urgent ? std::find_if(m_jobs.begin(), m_jobs.end(), [](const Job & j) { return !j.urgent; })
                       : m_jobs.end();
  m_jobs.insert(at, std::move(job));
}

void BuildQueue::post(QObject * owner, Work work, bool urgent) {
  if (!owner || !work)
    return;
  const bool wasIdle = idle();
  auto it = find(owner);
  if (it != m_jobs.end()) {
    it->work = std::move(work);
    if (urgent && !it->urgent) {
      Job j = std::move(*it);
      m_jobs.erase(it);
      j.urgent = true;
      insert(std::move(j));
    }
  } else {
    Job j{ owner, std::move(work), urgent, {} };
    j.gone = connect(owner, &QObject::destroyed, this, [this, owner] { cancel(owner); });
    insert(std::move(j));
  }
  noteIdle(wasIdle);
  schedule();
}

void BuildQueue::cancel(QObject * owner) {
  const bool wasIdle = idle();
  // the owner's, and any whose owner is gone (its pointer is cleared before `destroyed`)
  std::erase_if(m_jobs, [owner](Job & j) {
    if (!j.owner.isNull() && j.owner.data() != owner)
      return false;
    QObject::disconnect(j.gone);
    return true;
  });
  if (m_running.data() == owner)
    m_runningCancelled = true;
  if (!m_waiting.isNull() && m_waiting.data() == owner) {
    disconnect(m_waitGone);
    m_waiting.clear();
  }
  noteIdle(wasIdle);
  schedule();
}

void BuildQueue::resume(QObject * owner) {
  if (m_waiting.isNull() || m_waiting.data() != owner)
    return;
  const bool wasIdle = idle();
  disconnect(m_waitGone);
  m_waiting.clear();
  m_sinceStep.restart();        // the gap counts from the end of what it waited for
  noteIdle(wasIdle);
  schedule();
}

void BuildQueue::startAfter(int ms) {
  setHolding(true);
  m_hold.start(std::max(0, ms));
}

void BuildQueue::setGap(int ms) {
  ms = std::max(0, ms);
  if (ms == m_gap)
    return;
  m_gap = ms;
  emit gapChanged();
}

void BuildQueue::setHolding(bool holding) {
  if (holding == m_holding)
    return;
  m_holding = holding;
  emit holdingChanged();
}

void BuildQueue::noteIdle(bool wasIdle) {
  if (wasIdle != idle())
    emit idleChanged();
}

void BuildQueue::schedule(void) {
  if (m_jobs.empty() || m_timer.isActive())
    return;
  const Job & next = m_jobs.front();
  if (!next.urgent && (m_holding || !m_waiting.isNull()))
    return;                     // the hold's end, or resume(), schedules again
  // the gap since the last step's end: a step after a quiet while runs at once
  const qint64 since = m_sinceStep.elapsed();
  m_timer.start(int(std::max<qint64>(0, m_gap - since)));
}

void BuildQueue::runNext(void) {
  // the jobs of owners gone
  const bool wasIdle = idle();
  std::erase_if(m_jobs, [](const Job & j) { return j.owner.isNull(); });
  if (m_jobs.empty() || (!m_jobs.front().urgent && (m_holding || !m_waiting.isNull()))) {
    noteIdle(wasIdle);
    return;
  }

  Job job = std::move(m_jobs.front());
  m_jobs.pop_front();
  disconnect(job.gone);
  m_running = job.owner;
  m_runningCancelled = false;

  QElapsedTimer clock;
  clock.start();
  const Step result = job.work();
  m_lastStepNs = clock.nsecsElapsed();
  m_stepsRun++;

  QObject * owner = job.owner.data();
  m_running.clear();
  if (owner && !m_runningCancelled) {
    if (result == Step::More && !queued(owner)) {
      // before the jobs of its kind: a job finishes before the next begins
      auto at = job.urgent ? m_jobs.begin()
                           : std::find_if(m_jobs.begin(), m_jobs.end(), [](const Job & j) { return !j.urgent; });
      job.gone = connect(owner, &QObject::destroyed, this, [this, owner] { cancel(owner); });
      m_jobs.insert(at, std::move(job));
    } else if (result == Step::Waiting) {
      m_waiting = owner;
      // gone while waited for: the others go on
      m_waitGone = connect(owner, &QObject::destroyed, this, [this] {
        m_waiting.clear();
        if (idle())
          emit idleChanged();
        schedule();
      }, Qt::SingleShotConnection);
    }
  }
  m_sinceStep.restart();
  emit stepped();
  noteIdle(wasIdle);
  schedule();
}

// --- BuildJob ------------------------------------------------------------------

BuildJob::BuildJob(QObject * parent) : QObject(parent) {
  QMetaObject::invokeMethod(this, &BuildJob::componentComplete, Qt::QueuedConnection);
}

BuildJob::~BuildJob() {
  if (m_queue)
    m_queue->cancel(this);
}

void BuildJob::setQueue(BuildQueue * queue) {
  if (queue == m_queue)
    return;
  if (m_queue)
    m_queue->cancel(this);
  m_queue = queue;
  emit queueChanged();
  update();
}

void BuildJob::setWanted(bool wanted) {
  if (wanted == m_wanted)
    return;
  m_wanted = wanted;
  emit wantedChanged();
  update();
}

void BuildJob::setNeeded(bool needed) {
  if (needed == m_needed)
    return;
  m_needed = needed;
  emit neededChanged();
  update();
}

void BuildJob::setWaits(bool waits) {
  if (waits == m_waits)
    return;
  m_waits = waits;
  emit waitsChanged();
}

void BuildJob::componentComplete(void) {
  if (m_complete)
    return;
  m_complete = true;
  update();
}

void BuildJob::grant(void) {
  if (m_granted)
    return;
  m_granted = true;
  emit grantedChanged();
}

void BuildJob::update(void) {
  if (m_granted)
    return;
  if (m_needed) {                          // the user is looking: not a turn later
    if (m_queue)
      m_queue->cancel(this);
    grant();
    return;
  }
  if (!m_complete)
    return;
  if (!m_queue) {
    grant();                               // nothing to wait for
  } else if (m_wanted) {
    if (!m_queue->queued(this))
      m_queue->post(this, [this] {
        grant();
        return m_waits && !m_finished ? BuildQueue::Step::Waiting : BuildQueue::Step::Done;
      }, false);
  } else {
    m_queue->cancel(this);
  }
}

void BuildJob::finish(void) {
  if (m_finished)
    return;
  m_finished = true;
  if (m_queue)
    m_queue->resume(this);
}

// --- BatchedRows ---------------------------------------------------------------

BatchedRows::BatchedRows(QObject * parent) : QAbstractListModel(parent) {}

BatchedRows::~BatchedRows() {
  if (m_queue)
    m_queue->cancel(this);
  for (const QMetaObject::Connection & c : std::as_const(m_connections))
    disconnect(c);
}

int BatchedRows::rowCount(const QModelIndex & parent) const {
  return parent.isValid() ? 0 : m_count;
}

QVariant BatchedRows::data(const QModelIndex & index, int role) const {
  if (!m_source || !index.isValid() || index.row() >= m_count)
    return {};
  return m_source->data(m_source->index(index.row(), 0), role);
}

QHash<int, QByteArray> BatchedRows::roleNames(void) const {
  return m_source ? m_source->roleNames() : QHash<int, QByteArray>();
}

int BatchedRows::sourceCount(void) const {
  return m_source ? m_source->rowCount() : 0;
}

int BatchedRows::wantedCount(void) const {
  return m_active ? sourceCount() : 0;
}

bool BatchedRows::complete(void) const {
  return m_active && m_count == sourceCount();
}

void BatchedRows::sync(int oldCount) {
  if (oldCount != m_count)
    emit countChanged();
  const bool whole = complete();
  if (whole) {
    m_whole = true;
    if (m_queue)
      m_queue->cancel(this);
  }
  if (whole != m_wasComplete) {
    m_wasComplete = whole;
    emit completeChanged();
  }
}

void BatchedRows::grow(int rows) {
  rows = std::min(rows, wantedCount() - m_count);
  if (rows <= 0)
    return;
  const int old = m_count;
  beginInsertRows(QModelIndex(), m_count, m_count + rows - 1);
  m_count += rows;
  endInsertRows();
  sync(old);
}

void BatchedRows::shrinkTo(int rows) {
  if (rows >= m_count)
    return;
  const int old = m_count;
  beginRemoveRows(QModelIndex(), rows, m_count - 1);
  m_count = rows;
  endRemoveRows();
  sync(old);
}

BuildQueue::Step BatchedRows::step(void) {
  const int before = m_count;
  QElapsedTimer clock;
  clock.start();
  grow(m_batch);
  const qint64 took = clock.nsecsElapsed();
  // the next batch sized to the budget (a Repeater makes its delegates in the insert)
  if (m_count - before == m_batch) {
    if (took < BuildQueue::stepBudgetNs / 2)
      m_batch = std::min(m_batch * 2, 256);
    else if (took > BuildQueue::stepBudgetNs)
      m_batch = std::max(1, m_batch / 2);
  }
  return m_count < wantedCount() ? BuildQueue::Step::More : BuildQueue::Step::Done;
}

void BatchedRows::reconcile(void) {
  if (m_resetting || m_pendingInsert > 0 || m_pendingRemove > 0)
    return;
  const int wanted = wantedCount();
  if (m_count > wanted) {
    shrinkTo(wanted);
    return;
  }
  const int missing = wanted - m_count;
  if (missing == 0) {
    sync(m_count);
    return;
  }
  if (!m_queue) {
    /* every row at once, but only once the bindings have settled: QML may set
     * the source before the queue */
    if (!m_settling) {
      m_settling = true;
      QMetaObject::invokeMethod(this, [this] {
        m_settling = false;
        if (!m_queue)
          grow(wantedCount() - m_count);
      }, Qt::QueuedConnection);
    }
    return;
  }
  // a view on screen gets its first screen now; one that was whole keeps up with small changes
  if (m_shown || (m_whole && missing <= m_immediate))
    grow(std::min(missing, m_immediate));
  if (m_count < wantedCount())
    m_queue->post(this, [this] { return step(); }, m_shown);
  sync(m_count);
}

void BatchedRows::setSource(RowsModel * source) {
  if (source == m_source)
    return;
  for (const QMetaObject::Connection & c : std::as_const(m_connections))
    disconnect(c);
  m_connections.clear();
  if (m_queue)
    m_queue->cancel(this);

  const int old = m_count;
  beginResetModel();
  m_source = source;
  m_count = 0;
  m_whole = false;
  endResetModel();

  if (source) {
    using M = QAbstractItemModel;
    // inserts and removes within the rows handed over pass straight through
    m_connections << connect(source, &M::rowsAboutToBeInserted, this, [this](const QModelIndex & p, int first, int last) {
      if (p.isValid() || first >= m_count)
        return;                         // past them: reconcile() takes them on
      m_pendingInsert = last - first + 1;
      beginInsertRows(QModelIndex(), first, last);
    });
    m_connections << connect(source, &M::rowsInserted, this, [this](const QModelIndex & p, int, int) {
      if (p.isValid())
        return;
      const int old = m_count;
      if (m_pendingInsert > 0) {
        m_count += std::exchange(m_pendingInsert, 0);
        endInsertRows();
      }
      sync(old);
      reconcile();
    });
    m_connections << connect(source, &M::rowsAboutToBeRemoved, this, [this](const QModelIndex & p, int first, int last) {
      if (p.isValid() || first >= m_count)
        return;
      last = std::min(last, m_count - 1);
      m_pendingRemove = last - first + 1;
      beginRemoveRows(QModelIndex(), first, last);
    });
    m_connections << connect(source, &M::rowsRemoved, this, [this](const QModelIndex & p, int, int) {
      if (p.isValid())
        return;
      const int old = m_count;
      if (m_pendingRemove > 0) {
        m_count -= std::exchange(m_pendingRemove, 0);
        endRemoveRows();
      }
      sync(old);
      reconcile();
    });
    m_connections << connect(source, &M::dataChanged, this,
                             [this](const QModelIndex & tl, const QModelIndex & br, const QList<int> & roles) {
      const int last = std::min(br.row(), m_count - 1);
      if (tl.parent().isValid() || tl.row() > last)
        return;
      emit dataChanged(index(tl.row()), index(last), roles);
    });
    // anything else starts again from no rows (RowsModel sends none of these)
    auto resetStart = [this] {
      m_resetting = true;
      beginResetModel();
    };
    auto resetEnd = [this] {
      const int old = m_count;
      m_count = 0;
      endResetModel();
      m_resetting = false;
      if (m_queue)
        m_queue->cancel(this);
      sync(old);
      reconcile();
    };
    m_connections << connect(source, &M::modelAboutToBeReset, this, resetStart);
    m_connections << connect(source, &M::modelReset, this, resetEnd);
    m_connections << connect(source, &M::layoutAboutToBeChanged, this, resetStart);
    m_connections << connect(source, &M::layoutChanged, this, resetEnd);
    m_connections << connect(source, &M::rowsAboutToBeMoved, this, resetStart);
    m_connections << connect(source, &M::rowsMoved, this, resetEnd);
    m_connections << connect(source, &QObject::destroyed, this, [this] { setSource(nullptr); });
  }
  sync(old);
  emit sourceChanged();
  reconcile();
}

void BatchedRows::setQueue(BuildQueue * queue) {
  if (queue == m_queue)
    return;
  if (m_queue)
    m_queue->cancel(this);
  m_queue = queue;
  emit queueChanged();
  reconcile();
}

void BatchedRows::setActive(bool active) {
  if (active == m_active)
    return;
  m_active = active;
  if (!active) {
    m_whole = false;
    if (m_queue)
      m_queue->cancel(this);
  }
  emit activeChanged();
  reconcile();
}

void BatchedRows::setShown(bool shown) {
  if (shown == m_shown)
    return;
  m_shown = shown;
  emit shownChanged();
  reconcile();
}

void BatchedRows::setImmediate(int rows) {
  rows = std::max(0, rows);
  if (rows == m_immediate)
    return;
  m_immediate = rows;
  emit immediateChanged();
  reconcile();
}
