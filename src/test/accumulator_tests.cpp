#include "../../include/utreexo.h"
#include "../nodepool.h"
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <cstring>

BOOST_AUTO_TEST_SUITE(accumulator_tests)

using namespace utreexo;

void SetHash(Hash& hash, int num)
{
    for (uint8_t& byte : hash) {
        byte = 0;
    }

    hash[0] = num;
    hash[1] = num >> 8;
    hash[2] = num >> 16;
    hash[3] = num >> 24;
    hash[4] = 0xFF;
}

void CreateTestLeaves(std::vector<Leaf>& leaves, int count)
{
    for (int i = 0; i < count; i++) {
        Hash hash;
        SetHash(hash, i);
        leaves.emplace_back(std::move(hash), false);
    }
}

BOOST_AUTO_TEST_CASE(simple_add)
{
    Accumulator* full = (Accumulator*)new Pollard(0, 160);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 64);

    BOOST_CHECK(full->Modify(leaves, {}));

    delete full;
}

BOOST_AUTO_TEST_CASE(simple_full)
{
    Accumulator* full = (Accumulator*)new RamForest(0, 32);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 16);
    // Add test leaves, dont delete any.
    full->Modify(leaves, std::vector<uint64_t>());
    // Delete some leaves, dont add any new ones.
    leaves.clear();
    full->Modify(leaves, {0, 2, 3, 9});

    delete full;
}

BOOST_AUTO_TEST_CASE(simple_pruned)
{
    Accumulator* full = (Accumulator*)new Pollard(0, 64);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 32);
    // Remember all leaves
    for (Leaf& leaf : leaves)
        leaf.second = true;
    // Add test leaves, dont delete any.
    full->Modify(leaves, std::vector<uint64_t>());
    // Delete some leaves, dont add any new ones.
    leaves.clear();
    full->Modify(leaves, {0, 2, 3, 9});
    //full->PrintRoots();

    delete full;
}

BOOST_AUTO_TEST_CASE(batchproof_serialization)
{
    Accumulator* full = (Accumulator*)new RamForest(0, 64);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 32);

    full->Modify(leaves, std::vector<uint64_t>());

    std::vector<uint8_t> proof_bytes;
    BatchProof proof1;
    full->Prove(proof1, {leaves[0].first, leaves[1].first});
    proof1.Serialize(proof_bytes);

    BatchProof proof2;
    BOOST_CHECK(proof2.Unserialize(proof_bytes));
    BOOST_CHECK(proof1 == proof2);
}

BOOST_AUTO_TEST_CASE(singular_leaf_prove)
{
    Pollard pruned(0, 64);
    RamForest full(0, 64);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    // Add test leaves, dont delete any.
    full.Modify(leaves, {});
    pruned.Modify(leaves, {});
    //full.PrintRoots();

    for (Leaf& leaf : leaves) {
        BatchProof proof;
        full.Prove(proof, {leaf.first});
        BOOST_CHECK(pruned.Verify(proof, {leaf.first}));

        // Delete all cached leaves.
        pruned.Prune();
    }
}

BOOST_AUTO_TEST_CASE(simple_modified_proof)
{
    Pollard pruned(0, 64);
    RamForest full(0, 64);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    // Add test leaves, dont delete any.
    full.Modify(leaves, {});
    pruned.Modify(leaves, {});
    // full.PrintRoots();

    BatchProof proof;
    full.Prove(proof, {leaves[0].first});
    std::vector<Hash> modified_hashes = proof.GetHashes();
    // Fill the last hash with zeros.
    // This should cause verification to fail.
    modified_hashes.back().fill(0);
    BatchProof invalid(proof.GetTargets(), modified_hashes);
    // Assert that verification fails.
    BOOST_CHECK(pruned.Verify(invalid, {leaves[0].first}) == false);
}

BOOST_AUTO_TEST_CASE(simple_cached_proof)
{
    Pollard pruned(0, 64);
    RamForest full(0, 64);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    // Remember leaf 0 in the pollard.
    leaves[0].second = true;
    // Add test leaves, dont delete any.
    full.Modify(leaves, {});
    pruned.Modify(leaves, {});
    //full.PrintRoots();

    BatchProof proof;
    full.Prove(proof, {leaves[0].first});

    // Since the proof for leaf 0 is cached,
    // the proof can be any subset of the full proof.
    BOOST_CHECK(pruned.Verify(BatchProof(proof.GetTargets(), {proof.GetHashes()[0]}), {leaves[0].first}));
    BOOST_CHECK(pruned.Verify(BatchProof(proof.GetTargets(), {proof.GetHashes()[1]}), {leaves[0].first}));
    BOOST_CHECK(pruned.Verify(BatchProof(proof.GetTargets(), {proof.GetHashes()[2]}), {leaves[0].first}));

    BOOST_CHECK(pruned.Verify(BatchProof(proof.GetTargets(), {proof.GetHashes()[0], proof.GetHashes()[1]}), {leaves[0].first}));
    BOOST_CHECK(pruned.Verify(BatchProof(proof.GetTargets(), {proof.GetHashes()[0], proof.GetHashes()[2]}), {leaves[0].first}));
    BOOST_CHECK(pruned.Verify(BatchProof(proof.GetTargets(), {proof.GetHashes()[1], proof.GetHashes()[2]}), {leaves[0].first}));
    BOOST_CHECK(pruned.Verify(proof, {leaves[0].first}));
    // Empty proof should work since the pollard now holds the computed nodes as well.
    BOOST_CHECK(pruned.Verify(BatchProof(proof.GetTargets(), {}), {leaves[0].first}));
}

BOOST_AUTO_TEST_CASE(simple_batch_proof)
{
    Pollard pruned(0, 64);
    RamForest full(0, 64);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 15);

    // Add test leaves, dont delete any.
    full.Modify(leaves, {});
    pruned.Modify(leaves, {});
    //full.PrintRoots();

    BatchProof proof;
    full.Prove(proof, {leaves[0].first, leaves[7].first, leaves[8].first, leaves[14].first});

    BOOST_CHECK(pruned.Verify(proof, {leaves[0].first, leaves[7].first, leaves[8].first, leaves[14].first}));
}

// TODO: Add verification tests
// Modified proofs should not pass.
// Partial proof that are missing non cached hashes should not pass.
// Parital proofs that are only missing cached hashes should pass.

BOOST_AUTO_TEST_SUITE_END()
