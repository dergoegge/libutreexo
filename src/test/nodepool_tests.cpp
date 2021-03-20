#define protected public
#include "../nodepool.h"
#include <boost/test/unit_test.hpp>

using namespace utreexo;

class TestNode
{
public:
    Accumulator::NodePtr<TestNode> next;

    void NodePoolDestroy()
    {
        next = nullptr;
    }
};

BOOST_AUTO_TEST_SUITE(nodepool_tests)

BOOST_AUTO_TEST_CASE(simple)
{
    Accumulator::NodePool<TestNode> pool(16);
    // Create a vector of NodePtr and take all nodes from the pool.
    std::vector<Accumulator::NodePtr<TestNode>> nodes;
    for (int i = 0; i < pool.Capacity(); i++) {
        Accumulator::NodePtr<TestNode> node(&pool);
        nodes.push_back(node);
    }

    // Check that all elements are taken.
    BOOST_CHECK(pool.Take() == NULL);
    BOOST_CHECK(pool.Size() == 16);
    BOOST_CHECK(pool.Capacity() == 16);

    for (int i = 0; i < pool.Capacity(); i++) {
        // Check that all reference counters are set to 1.
        BOOST_CHECK(nodes[i].RefCount() == 1);
        // Create a new node pointer and check that the reference count is now 2.
        Accumulator::NodePtr<TestNode> tmp = nodes[i];
        BOOST_CHECK(tmp.RefCount() == 2);
    }

    {
        // Test chaining multiple assignements.
        Accumulator::NodePtr<TestNode> tmp[8];
        tmp[0] = tmp[1] = tmp[2] = tmp[3] = tmp[4] = tmp[5] = tmp[6] = tmp[7] = nodes[0];
        // 1 ref in nodes and 8 through tmp = 9
        BOOST_CHECK(nodes[0].RefCount() == 9);
    }

    {
        // Let one node survive the clear.
        Accumulator::NodePtr<TestNode> survivor = nodes[0];
        nodes.clear();
        // Only the survivor should be left in the pool.
        BOOST_CHECK(pool.Size() == 1);
    }

    // Now that the survivor went out of scope, the pool should be emtpy.
    BOOST_CHECK(pool.Size() == 0);
}

BOOST_AUTO_TEST_CASE(chain)
{
    Accumulator::NodePool<TestNode> pool(16);
    Accumulator::NodePtr<TestNode> head(&pool);
    Accumulator::NodePtr<TestNode> curr = head;
    while (pool.Size() < pool.Capacity()) {
        curr->next = Accumulator::NodePtr<TestNode>(&pool);
        curr = curr->next;
    }

    BOOST_CHECK(pool.Size() == pool.Capacity());

    // Delete the head.
    head = nullptr;
    curr = nullptr;

    BOOST_CHECK(pool.Size() == 0);
}

void vector_test(std::vector<Accumulator::NodePtr<TestNode>> nodes)
{
    for (Accumulator::NodePtr<TestNode>& node : nodes) {
        BOOST_CHECK(node.RefCount() == 2);
    }
}

BOOST_AUTO_TEST_CASE(vectors)
{
    Accumulator::NodePool<TestNode> pool(16);

    std::vector<Accumulator::NodePtr<TestNode>> nodes;
    for (int i = 0; i < pool.Capacity(); i++) {
        nodes.emplace_back(&pool);
    }

    for (Accumulator::NodePtr<TestNode>& node : nodes) {
        BOOST_CHECK(node.RefCount() == 1);
    }

	vector_test(nodes);

    auto copy = nodes;

    for (Accumulator::NodePtr<TestNode>& node : nodes) {
        BOOST_CHECK(node.RefCount() == 2);
    }
    copy.clear();

    for (Accumulator::NodePtr<TestNode>& node : nodes) {
        BOOST_CHECK(node.RefCount() == 1);
    }

    BOOST_CHECK(pool.Size() == pool.Capacity());
}

BOOST_AUTO_TEST_SUITE_END()
#undef protected
