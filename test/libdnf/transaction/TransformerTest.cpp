#include <json.h>
#include <set>
#include <sstream>
#include <string>

#include "libdnf/transaction/RPMItem.hpp"
#include "libdnf/transaction/Swdb.hpp"
#include "libdnf/transaction/transaction.hpp"
#include "libdnf/transaction/transaction_item.hpp"
#include "libdnf/transaction/Types.hpp"

#include "TransformerTest.hpp"

using namespace libdnf::transaction;

CPPUNIT_TEST_SUITE_REGISTRATION(TransformerTest);

TransformerMock::TransformerMock()
  : Transformer("", "")
{
}

static const char *create_history_sql =
#include "sql/create_test_history_db.sql"
    ;

static const char *groups_json =
#include "assets/groups.json"
    ;

void
TransformerTest::setUp()
{
    swdb = std::make_shared< libdnf::utils::SQLite3 >(":memory:");
    history = std::make_shared< libdnf::utils::SQLite3 >(":memory:");
    Transformer::createDatabase(swdb);
    history.get()->exec(create_history_sql);
}

void
TransformerTest::tearDown()
{
}

void
TransformerTest::testGroupTransformation()
{
    // load test groups.json
    struct json_object *groupsJson = json_tokener_parse (groups_json);

    // perform the transformation
    transformer.processGroupPersistor(swdb, groupsJson);

    swdb->backup("db.sql");

    // check basic stuff in generated transaction
    Transaction trans(swdb, 1);
    CPPUNIT_ASSERT_EQUAL((int64_t)1, trans.get_id());
    CPPUNIT_ASSERT_EQUAL(TransactionState::DONE, trans.get_state());

    // load transaction items
    auto items = trans.getItems();
    CPPUNIT_ASSERT_EQUAL(2, static_cast< int >(items.size()));

    // verify items
    for (auto transItem : items) {
        auto item = transItem->getItem();
        auto type = item->getItemType();
        if (type == ItemType::GROUP) {
            auto group = std::dynamic_pointer_cast< CompsGroupItem >(item);

            CPPUNIT_ASSERT(group->getGroupId() == "core");
            CPPUNIT_ASSERT("Core" == group->getName());
            CPPUNIT_ASSERT("Úplný základ" == group->getTranslatedName());

            auto packages = group->getPackages();

            CPPUNIT_ASSERT(1 == packages.size());

            auto groupPkg = packages[0];
            CPPUNIT_ASSERT(groupPkg->getName() == "dnf-yum");
            CPPUNIT_ASSERT(groupPkg->getInstalled() == true);
            CPPUNIT_ASSERT(groupPkg->getPackageType() == CompsPackageType::MANDATORY);

        } else if (type == ItemType::ENVIRONMENT) {
            auto env = std::dynamic_pointer_cast< CompsEnvironmentItem >(item);
            CPPUNIT_ASSERT("minimal-environment" == env->getEnvironmentId());
            CPPUNIT_ASSERT("Minimal Install" == env->getName());
            CPPUNIT_ASSERT("Minimálna inštalácia" == env->getTranslatedName());

            auto groups = env->getGroups();
            CPPUNIT_ASSERT(1 == groups.size());

            auto envGroup = groups[0];
            CPPUNIT_ASSERT(envGroup->getGroupId() == "core");
            CPPUNIT_ASSERT(envGroup->getInstalled() == true);
            CPPUNIT_ASSERT(envGroup->getGroupType() == CompsPackageType::MANDATORY);

        } else {
            CPPUNIT_FAIL("Invalid item type: " + std::to_string(static_cast< int >(type)));
        }
    }

    json_object_put(groupsJson);
}

void
TransformerTest::testTransformTrans()
{
    // perform database transformation
    transformer.transformTrans(swdb, history);

    // check first transaction attributes
    Transaction first(swdb, 1);
    CPPUNIT_ASSERT(first.get_id() == 1);
    CPPUNIT_ASSERT(first.get_dt_begin() == 1513267401);
    CPPUNIT_ASSERT(first.get_dt_end() == 1513267509);
    CPPUNIT_ASSERT(first.get_rpmdb_version_begin() == "2213:9795b6a4db5e5368628b5240ec63a629833c5594");
    CPPUNIT_ASSERT(first.get_rpmdb_version_end() == "2213:9eab991133c166f8bcf3ecea9fb422b853f7aebc");
    CPPUNIT_ASSERT(first.get_releasever() == "26");
    CPPUNIT_ASSERT(first.get_user_id() == 1000);
    CPPUNIT_ASSERT(first.get_cmdline() == "upgrade -y");
    CPPUNIT_ASSERT(first.get_state() == TransactionState::DONE);

    // check first transaction output
    auto firstOut = first.getConsoleOutput();
    CPPUNIT_ASSERT(firstOut.size() == 2);
    CPPUNIT_ASSERT(firstOut[0].first == 1);
    CPPUNIT_ASSERT(firstOut[0].second == "line1");
    CPPUNIT_ASSERT(firstOut[1].first == 1);
    CPPUNIT_ASSERT(firstOut[1].second == "line2");

    // check software performed with
    auto firstSoftWith = first.getSoftwarePerformedWith();
    CPPUNIT_ASSERT(firstSoftWith.size() == 1);
    for (auto soft : firstSoftWith) {
        CPPUNIT_ASSERT(soft->getId() == 2);
    }

    // check first transaction items
    auto items = first.getItems();
    CPPUNIT_ASSERT(items.size() == 2);
    for (auto item : items) {
        auto rpm = std::dynamic_pointer_cast< RPMItem >(item->getItem());
        if (rpm->getName() == "chrony" && rpm->getVersion() == "3.1") {
            CPPUNIT_ASSERT(rpm->getEpoch() == 1);
            CPPUNIT_ASSERT(rpm->getRelease() == "4.fc26");
            CPPUNIT_ASSERT(item->get_action() == TransactionItemAction::UPGRADED);
            CPPUNIT_ASSERT(item->get_reason() == TransactionItemReason::USER);
            CPPUNIT_ASSERT(item->get_state() == TransactionItemState::DONE);

            // TODO repo, replaced
        } else { // chrony 3.2
            CPPUNIT_ASSERT(rpm->getEpoch() == 1);
            CPPUNIT_ASSERT(rpm->getVersion() == "3.2");
            CPPUNIT_ASSERT(rpm->getRelease() == "4.fc26");

            CPPUNIT_ASSERT(item->get_action() == TransactionItemAction::UPGRADE);
            CPPUNIT_ASSERT(item->get_reason() == TransactionItemReason::USER);
            CPPUNIT_ASSERT(item->get_state() == TransactionItemState::DONE);
            // TODO repo, replaced
        }
    }

    // check second transaction attributes
    Transaction second(swdb, 2);
    CPPUNIT_ASSERT(second.get_id() == 2);
    CPPUNIT_ASSERT(second.get_dt_begin() == 1513267535);
    CPPUNIT_ASSERT(second.get_dt_end() == 1513267539);
    CPPUNIT_ASSERT(second.get_rpmdb_version_begin() ==
                   "2213:9eab991133c166f8bcf3ecea9fb422b853f7aebc");
    CPPUNIT_ASSERT(second.get_rpmdb_version_end() == "2214:e02004142740afb5b6d148d50bc84be4ab41ad13");
    CPPUNIT_ASSERT(second.get_user_id() == 1000);
    CPPUNIT_ASSERT(second.get_cmdline() == "-y install Foo");
    CPPUNIT_ASSERT(second.get_state() == TransactionState::DONE);

    // check second transaction console output
    auto secondOut = second.getConsoleOutput();
    CPPUNIT_ASSERT(secondOut.size() == 2);
    CPPUNIT_ASSERT(secondOut[0].first == 2);
    CPPUNIT_ASSERT(secondOut[0].second == "msg1");
    CPPUNIT_ASSERT(secondOut[1].first == 2);
    CPPUNIT_ASSERT(secondOut[1].second == "msg2");

    // check second transaction performed with software
    std::set< int64_t > possibleValues = {2, 3};
    auto secondSoftWith = second.getSoftwarePerformedWith();
    CPPUNIT_ASSERT(secondSoftWith.size() == 2);
    for (auto soft : secondSoftWith) {
        auto it = possibleValues.find(soft->getId());
        CPPUNIT_ASSERT(it != possibleValues.end());
        possibleValues.erase(it);
    }

    // check second transaction items
    items = second.getItems();
    CPPUNIT_ASSERT(items.size() == 1);
    for (auto item : items) {
        auto kernel = std::dynamic_pointer_cast< RPMItem >(item->getItem());
        CPPUNIT_ASSERT(kernel->getName() == "kernel");
        CPPUNIT_ASSERT(kernel->getEpoch() == 0);
        CPPUNIT_ASSERT(kernel->getVersion() == "4.11");
        CPPUNIT_ASSERT(kernel->getRelease() == "301.fc26");

        CPPUNIT_ASSERT(item->get_action() == TransactionItemAction::INSTALL);
        CPPUNIT_ASSERT(item->get_reason() == TransactionItemReason::DEPENDENCY);
        CPPUNIT_ASSERT(item->get_state() == TransactionItemState::DONE);
    }

    swdb->backup("sql.db");
}
