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
#include "test_builds.h"

#include "buildqueue.h"
#include "rowsmodel.h"

#include <QAbstractItemModelTester>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QStringList>
#include <QTest>

#include <memory>
#include <vector>

namespace {

  using Step = BuildQueue::Step;

  // rows of a keyed model: the key, and a name that may differ
  std::vector<RowsModel::Row> rowsOf(const std::vector<int> & keys, const QString & suffix = QString()) {
    std::vector<RowsModel::Row> rows;
    for (int k : keys)
      rows.push_back({ k, QStringLiteral("S%1").arg(k) + suffix });
    return rows;
  }
  std::vector<int> range(int from, int to) {
    std::vector<int> keys;
    for (int k = from; k < to; k++)
      keys.push_back(k);
    return keys;
  }

  struct Rows {
    RowsModel model{ { "shape", "name" } };
    Rows(const std::vector<int> & keys) { model.setKeyed(true); model.setRows(rowsOf(keys)); }
  };

  // a view's model, checked by Qt's model tester through every change
  struct View {
    BatchedRows rows;
    QAbstractItemModelTester tester{ &rows, QAbstractItemModelTester::FailureReportingMode::QtTest };
  };

  QStringList namesIn(const QAbstractItemModel & m) {
    QStringList names;
    for (int i = 0; i < m.rowCount(); i++)
      names << m.data(m.index(i, 0), Qt::UserRole + 1).toString();
    return names;
  }

}

void TestBuilds::nothingButTheUrgentRunsBeforeTheStart() {
  BuildQueue q;
  q.setGap(0);
  QVERIFY(q.holding());
  QObject a, b;
  int ranA = 0, ranB = 0;
  q.post(&a, [&] { ranA++; return Step::Done; }, false);
  q.post(&b, [&] { ranB++; return Step::Done; }, true);
  QTRY_COMPARE(ranB, 1);
  QTest::qWait(50);
  QCOMPARE(ranA, 0);
  QVERIFY(!q.idle());
  q.startAfter(30);
  QVERIFY(q.holding());
  QTRY_COMPARE(ranA, 1);
  QTRY_VERIFY(q.idle());
}

void TestBuilds::stepsRunOneAtATimeAGapApart() {
  BuildQueue q;
  q.setGap(30);
  q.startAfter(0);
  QObject owners[3];
  QElapsedTimer clock;
  clock.start();
  std::vector<std::pair<int, qint64>> ran;
  for (int i = 0; i < 3; i++)
    q.post(&owners[i], [&, i] { ran.push_back({ i, clock.elapsed() }); return Step::Done; }, false);
  QTRY_COMPARE(int(ran.size()), 3);
  for (int i = 0; i < 3; i++)
    QCOMPARE(ran[size_t(i)].first, i);                      // in the order posted
  for (int i = 1; i < 3; i++)
    QVERIFY2(ran[size_t(i)].second - ran[size_t(i - 1)].second >= 25,
             qPrintable(QStringLiteral("%1 ms apart").arg(ran[size_t(i)].second - ran[size_t(i - 1)].second)));
  QCOMPARE(q.stepsRun(), 3);
}

void TestBuilds::aJobWithMoreStepsFinishesBeforeTheNext() {
  BuildQueue q;
  q.setGap(0);
  q.startAfter(0);
  QObject a, b;
  QStringList order;
  int left = 3;
  q.post(&a, [&] { order << "a"; return --left > 0 ? Step::More : Step::Done; }, false);
  q.post(&b, [&] { order << "b"; return Step::Done; }, false);
  QTRY_VERIFY(q.idle());
  QCOMPARE(order.join(' '), QStringLiteral("a a a b"));
}

void TestBuilds::aWaitingStepHoldsTheOthersUntilItResumes() {
  BuildQueue q;
  q.setGap(0);
  q.startAfter(0);
  QObject a, b, c;
  QStringList order;
  q.post(&a, [&] { order << "a"; return Step::Waiting; }, false);
  q.post(&b, [&] { order << "b"; return Step::Done; }, false);
  QTRY_COMPARE(order.join(' '), QStringLiteral("a"));
  q.post(&c, [&] { order << "c"; return Step::Done; }, true);   // the urgent do not wait
  QTRY_COMPARE(order.join(' '), QStringLiteral("a c"));
  QTest::qWait(50);
  QCOMPARE(order.join(' '), QStringLiteral("a c"));
  QVERIFY(!q.idle());
  q.resume(&b);                                                 // not the one waited for
  QTest::qWait(20);
  QCOMPARE(order.join(' '), QStringLiteral("a c"));
  q.resume(&a);
  QTRY_COMPARE(order.join(' '), QStringLiteral("a c b"));
  QTRY_VERIFY(q.idle());
}

void TestBuilds::aJobGoesWithItsOwner() {
  BuildQueue q;
  q.setGap(0);
  auto a = std::make_unique<QObject>();
  int ran = 0;
  q.post(a.get(), [&] { ran++; return Step::Done; }, false);
  QVERIFY(q.queued(a.get()));
  QSignalSpy idle(&q, &BuildQueue::idleChanged);
  a.reset();
  QVERIFY(q.idle());
  QCOMPARE(idle.count(), 1);
  q.startAfter(0);
  QTest::qWait(30);
  QCOMPARE(ran, 0);
  // and one waited for, gone, waits no more
  auto w = std::make_unique<QObject>();
  QObject b;
  q.post(w.get(), [] { return Step::Waiting; }, false);
  q.post(&b, [&] { ran++; return Step::Done; }, false);
  QTRY_VERIFY(!q.queued(w.get()));
  w.reset();
  QTRY_COMPARE(ran, 1);
}

void TestBuilds::aNeededJobIsGrantedAtOnce() {
  BuildQueue q;                                // held
  BuildJob job;
  job.setQueue(&q);
  job.componentComplete();
  QVERIFY(!job.granted());
  QVERIFY(q.queued(&job));
  QSignalSpy granted(&job, &BuildJob::grantedChanged);
  job.setNeeded(true);
  QVERIFY(job.granted());                      // in the call, not on a later turn
  QCOMPARE(granted.count(), 1);
  QVERIFY(!q.queued(&job));
  job.setNeeded(false);
  QVERIFY(job.granted());                      // and kept
}

void TestBuilds::aQueuedJobIsGrantedInTurn() {
  BuildQueue q;
  q.setGap(0);
  BuildJob first, second;
  for (BuildJob * j : { &first, &second }) {
    j->setQueue(&q);
    j->setWaits(true);
    j->componentComplete();
  }
  q.startAfter(0);
  QTRY_VERIFY(first.granted());
  QTest::qWait(30);
  QVERIFY2(!second.granted(), "the queue waits for the first's finish()");
  first.finish();
  QTRY_VERIFY(second.granted());
  second.finish();
  QTRY_VERIFY(q.idle());
  // unwanted, it is not queued
  BuildJob third;
  third.setWanted(false);
  third.setQueue(&q);
  third.componentComplete();
  QVERIFY(!q.queued(&third));
}

void TestBuilds::aShownViewGetsItsFirstScreenAtOnce() {
  BuildQueue q;                                // held: what shows does not wait for it
  q.setGap(0);
  Rows source(range(0, 100));
  View v;
  v.rows.setQueue(&q);
  v.rows.setShown(true);
  v.rows.setSource(&source.model);
  QCOMPARE(v.rows.count(), v.rows.immediate());
  QVERIFY(!v.rows.complete());
  QTRY_VERIFY(v.rows.complete());
  QCOMPARE(v.rows.count(), 100);
  QCOMPARE(namesIn(v.rows), namesIn(source.model));
}

void TestBuilds::aHiddenViewWaitsForTheQueue() {
  BuildQueue q;
  q.setGap(0);
  Rows source(range(0, 100));
  View v;
  v.rows.setQueue(&q);
  v.rows.setSource(&source.model);
  QTest::qWait(30);
  QCOMPARE(v.rows.count(), 0);
  QSignalSpy steps(&q, &BuildQueue::stepped);
  QSignalSpy complete(&v.rows, &BatchedRows::completeChanged);
  q.startAfter(0);
  QTRY_VERIFY(v.rows.complete());
  QCOMPARE(complete.count(), 1);
  QVERIFY2(steps.count() > 1, "a batch at a time");
  QCOMPARE(namesIn(v.rows), namesIn(source.model));
  // shown, a hidden one that is not whole yet is urgent
  Rows other(range(0, 100));
  View w;
  BuildQueue held;
  w.rows.setQueue(&held);
  w.rows.setSource(&other.model);
  QCOMPARE(w.rows.count(), 0);
  w.rows.setShown(true);
  QCOMPARE(w.rows.count(), w.rows.immediate());
  QTRY_VERIFY(w.rows.complete());
}

void TestBuilds::aWholeViewKeepsUpWithSmallChanges() {
  BuildQueue q;
  q.setGap(0);
  q.startAfter(0);
  Rows source(range(0, 10));
  View v;
  v.rows.setQueue(&q);
  v.rows.setSource(&source.model);
  QTRY_VERIFY(v.rows.complete());

  QSignalSpy inserted(&v.rows, &QAbstractItemModel::rowsInserted);
  QSignalSpy removed(&v.rows, &QAbstractItemModel::rowsRemoved);
  QSignalSpy changed(&v.rows, &QAbstractItemModel::dataChanged);
  // a row in the middle: one insert there, at once (hidden, the queue not asked)
  std::vector<int> keys = range(0, 10);
  keys.insert(keys.begin() + 4, 50);
  source.model.setRows(rowsOf(keys));
  QCOMPARE(v.rows.count(), 11);
  QCOMPARE(inserted.count(), 1);
  QCOMPARE(inserted.at(0).at(1).toInt(), 4);
  QCOMPARE(inserted.at(0).at(2).toInt(), 4);
  QCOMPARE(changed.count(), 0);
  QVERIFY(!q.queued(&v.rows));
  // and taken away
  source.model.setRows(rowsOf(range(0, 10)));
  QCOMPARE(v.rows.count(), 10);
  QCOMPARE(removed.count(), 1);
  QCOMPARE(removed.at(0).at(1).toInt(), 4);
  // a row that changes is the one updated
  std::vector<RowsModel::Row> rows = rowsOf(range(0, 10));
  rows[7][1] = QStringLiteral("renamed");
  source.model.setRows(std::move(rows));
  QCOMPARE(changed.count(), 1);
  QCOMPARE(changed.at(0).at(0).toModelIndex().row(), 7);
  QCOMPARE(namesIn(v.rows), namesIn(source.model));
}

void TestBuilds::manyRowsAtOnceComeInBatchesAgain() {
  BuildQueue q;
  q.setGap(0);
  q.startAfter(0);
  Rows source(range(0, 10));
  View v;
  v.rows.setQueue(&q);
  v.rows.setSource(&source.model);
  QTRY_VERIFY(v.rows.complete());
  source.model.setRows(rowsOf(range(0, 200)));
  QCOMPARE(v.rows.count(), 10);                // hidden: all of it through the queue
  QVERIFY(q.queued(&v.rows));
  QTRY_VERIFY(v.rows.complete());
  QCOMPARE(namesIn(v.rows), namesIn(source.model));
  // and a row taken from the part handed over while the rest is still coming
  source.model.setRows(rowsOf(range(0, 10)));
  QCOMPARE(v.rows.count(), 10);
  source.model.setRows(rowsOf(range(0, 300)));
  std::vector<int> keys = range(0, 300);
  keys.erase(keys.begin() + 2);
  source.model.setRows(rowsOf(keys));
  QCOMPARE(v.rows.count(), 9);
  QTRY_VERIFY(v.rows.complete());
  QCOMPARE(v.rows.count(), 299);
  QCOMPARE(namesIn(v.rows), namesIn(source.model));
}

void TestBuilds::anInactiveViewHasNoRows() {
  BuildQueue q;
  q.setGap(0);
  q.startAfter(0);
  Rows source(range(0, 40));
  View v;
  v.rows.setActive(false);
  v.rows.setQueue(&q);
  v.rows.setShown(true);
  v.rows.setSource(&source.model);
  QTest::qWait(20);
  QCOMPARE(v.rows.count(), 0);
  QVERIFY(!v.rows.complete());
  source.model.setRows(rowsOf(range(0, 45)));
  QCOMPARE(v.rows.count(), 0);
  v.rows.setActive(true);
  QCOMPARE(v.rows.count(), v.rows.immediate());
  QTRY_VERIFY(v.rows.complete());
  v.rows.setActive(false);
  QCOMPARE(v.rows.count(), 0);
  QVERIFY(!q.queued(&v.rows));
  // without a queue, every row a turn later: QML may set the queue after the source
  View all;
  all.rows.setSource(&source.model);
  QCOMPARE(all.rows.count(), 0);
  all.rows.setQueue(&q);
  all.rows.setShown(true);
  QCOMPARE(all.rows.count(), all.rows.immediate());   // the queue's way, now it has one
  View none;
  none.rows.setSource(&source.model);
  QTRY_COMPARE(none.rows.count(), 45);
}

void TestBuilds::keyedRowsInsertAndRemoveWhereTheyStand() {
  RowsModel m({ "shape", "name" });
  QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
  m.setKeyed(true);
  m.setRows(rowsOf({ 1, 2, 3 }));
  QSignalSpy inserted(&m, &QAbstractItemModel::rowsInserted);
  QSignalSpy removed(&m, &QAbstractItemModel::rowsRemoved);
  QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);
  QSignalSpy revision(&m, &RowsModel::revisionChanged);
  m.setRows(rowsOf({ 0, 1, 5, 6, 2, 3 }));
  QCOMPARE(inserted.count(), 2);                       // a run is one insert
  QCOMPARE(inserted.at(0).at(1).toInt(), 0);
  QCOMPARE(inserted.at(1).at(1).toInt(), 2);
  QCOMPARE(inserted.at(1).at(2).toInt(), 3);
  QCOMPARE(changed.count(), 0);
  QCOMPARE(revision.count(), 1);
  m.setRows(rowsOf({ 1, 2, 3 }));
  QCOMPARE(removed.count(), 2);
  QCOMPARE(changed.count(), 0);
  QCOMPARE(namesIn(m), QStringList({ "S1", "S2", "S3" }));
  m.setRows(rowsOf({ 1, 2, 3 }));
  QCOMPARE(revision.count(), 2);                       // nothing changed, nothing said
  m.setRows(rowsOf({}));
  QCOMPARE(m.count(), 0);
  m.setRows(rowsOf({ 4, 7 }));
  QCOMPARE(namesIn(m), QStringList({ "S4", "S7" }));
}

void TestBuilds::keyedRowsOutOfOrderFallBackToPositions() {
  RowsModel m({ "shape", "name" });
  QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
  m.setKeyed(true);
  m.setRows(rowsOf({ 1, 2, 3 }));
  QSignalSpy inserted(&m, &QAbstractItemModel::rowsInserted);
  QSignalSpy removed(&m, &QAbstractItemModel::rowsRemoved);
  m.setRows(rowsOf({ 3, 2, 1 }));
  QCOMPARE(inserted.count(), 0);
  QCOMPARE(removed.count(), 0);
  QCOMPARE(namesIn(m), QStringList({ "S3", "S2", "S1" }));
  m.setRows(rowsOf({ 2, 2 }));                         // not unique
  QCOMPARE(namesIn(m), QStringList({ "S2", "S2" }));
}

void TestBuilds::rowsGoAndComeAtTheirPlaces() {
  // the Solver's list: rows removed and inserted where its merge says, a few
  // runs one signal each, many in one pass; the rows come out the same
  for (const int spread : { 1, 3, 40 }) {
    RowsModel m({ "shape", "name" });
    QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
    std::vector<int> want = range(0, 200);
    m.setRows(rowsOf(want));
    QSignalSpy inserted(&m, &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(&m, &QAbstractItemModel::rowsRemoved);
    QSignalSpy count(&m, &RowsModel::countChanged);

    // every 5th row gone, as many runs as `spread` allows
    std::vector<int> gone;
    for (int r = 0; r < 200 && int(gone.size()) < spread * 2; r += 5) {
      gone.push_back(r);
      if (spread == 1)
        for (int k = 1; k < 4; k++)
          gone.push_back(r + k);
    }
    for (auto it = gone.rbegin(); it != gone.rend(); ++it)
      want.erase(want.begin() + *it);
    m.removeRowsAt(gone);
    QCOMPARE(m.count(), int(want.size()));
    QCOMPARE(count.count(), 1);
    auto runs = [](const std::vector<int> & rows) {
      int n = 1;
      for (size_t i = 1; i < rows.size(); i++)
        n += rows[i] != rows[i - 1] + 1 ? 1 : 0;
      return n <= RowsModel::kRuns ? n : 1;
    };
    QCOMPARE(removed.count(), runs(gone));

    // new rows at places of the list they make: 1000 + their place
    std::vector<int> at;
    const int before = int(want.size());
    for (int r = 0; r <= before + int(at.size()) && int(at.size()) < spread * 3; r += 7) {
      at.push_back(r);
      if (spread == 1)
        at.push_back(r + 1);
    }
    std::vector<RowsModel::Row> values;
    for (int r : at) {
      want.insert(want.begin() + r, 1000 + r);
      values.push_back({ 1000 + r, QStringLiteral("S%1").arg(1000 + r) });
    }
    m.insertRowsAt(at, std::move(values));
    QStringList names;
    for (int k : want)
      names << QStringLiteral("S%1").arg(k);
    QCOMPARE(namesIn(m), names);
    QCOMPARE(count.count(), 2);
    QCOMPARE(inserted.count(), runs(at));
  }
}
