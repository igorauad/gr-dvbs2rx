/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cdeque.h"
#include <volk/volk_alloc.hh>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

namespace bdata = boost::unit_test::data;

namespace gr {
namespace dvbs2rx {

// Test counterclockwise queue movement by pushing new elements on the back of
// the queue. Do so for a varying number of L-length segment repetitions.
BOOST_DATA_TEST_CASE(test_queue_push_back, bdata::make({ 1, 10, 100 }), n_reps)
{
    unsigned int len = 4;
    cdeque<float> q(len, n_reps);

    // Random vector with the same length as the queue.
    volk::vector<float> vec(len);
    for (size_t i = 0; i < vec.size(); ++i) {
        vec[i] = rand();
    }

    // The front element of the queue remains 0 while filling the first L-1
    // elements from vec.
    for (size_t i = 0; i < vec.size() - 1; ++i) {
        q.push_back(vec[i]);
        BOOST_CHECK(q.back() == vec[i]);
        BOOST_CHECK(q.front() == 0);
    }

    // When the last element from vec is pushed, the queue becomes full, and the
    // oldest element (vec[0]) appears at the head of the queue.
    q.push_back(vec[len - 1]);
    BOOST_CHECK(q.front() == vec[0]);

    // Now that the queue is full, whenever a new element is pushed into it, the
    // oldest element should be thrown away.
    for (size_t i = 1; i < vec.size(); ++i) {
        float val = rand();
        q.push_back(val);
        BOOST_CHECK(q.back() == val);
        BOOST_CHECK(q.front() == vec[i]);
    }
}

// Test clockwise queue movement by pushing new elements at the front of the
// queue. Do so for a varying number of L-length segment repetitions.
BOOST_DATA_TEST_CASE(test_queue_push_front, bdata::make({ 1, 10, 100 }), n_reps)
{
    unsigned int len = 4;
    cdeque<float> q(len, n_reps);

    // Random vector with the same length as the queue.
    volk::vector<float> vec(len);
    for (size_t i = 0; i < vec.size(); ++i) {
        vec[i] = rand();
    }

    // The back element of the queue remains 0 while filling the first L-1
    // elements from vec.
    for (size_t i = 0; i < vec.size() - 1; ++i) {
        q.push_front(vec[i]);
        BOOST_CHECK(q.back() == 0);
        BOOST_CHECK(q.front() == vec[i]);
    }

    // When the last element from vec is pushed, the queue becomes full, and the
    // oldest element (vec[0]) appears at the tail of the queue.
    q.push_front(vec[len - 1]);
    BOOST_CHECK(q.back() == vec[0]);

    // Now that the queue is full, whenever a new element is pushed into it, the
    // oldest element should be thrown away.
    for (size_t i = 1; i < vec.size(); ++i) {
        float val = rand();
        q.push_front(val);
        BOOST_CHECK(q.back() == vec[i]);
        BOOST_CHECK(q.front() == val);
    }
}

// Check the queue's behavior in comparison to a regular ring buffer while
// writing elements on the back/tail of the queue (counterclockwise movement).
BOOST_DATA_TEST_CASE(test_cdeque_vs_ring_buffer_ccw, bdata::make({ 1, 10, 100 }), n_reps)
{
    // queue object
    unsigned int len = 4;
    cdeque<int> q(len, n_reps);

    // Ordinary ring buffer
    std::vector<int> ring_buffer(len);
    unsigned int i_tail = 0;
    unsigned int i_head = len - 1;

    // Test an arbitrarily large number of samples
    int n_samples = n_reps * len * 100;
    int i_sample = 0;
    while (i_sample < n_samples) {
        int val = rand();
        ring_buffer[i_tail] = val;
        q.push_back(val);
        BOOST_CHECK(q.back() == ring_buffer[i_tail]);
        BOOST_CHECK(q.front() == ring_buffer[i_head]);
        i_tail = (i_tail - 1) % len;
        i_head = (i_head - 1) % len;
        i_sample++;
    }
}

// Check the queue's behavior in comparison to a regular ring buffer while
// writing elements on the front/head of the queue (clockwise movement).
BOOST_DATA_TEST_CASE(test_cdeque_vs_ring_buffer_cw, bdata::make({ 1, 10, 100 }), n_reps)
{
    // queue object
    unsigned int len = 4;
    cdeque<int> q(len, n_reps);

    // Ordinary ring buffer
    std::vector<int> ring_buffer(len);
    unsigned int i_tail = 0;
    unsigned int i_head = len - 1;

    // Test an arbitrarily large number of samples
    int n_samples = n_reps * len * 100;
    int i_sample = 0;
    while (i_sample < n_samples) {
        int val = rand();
        ring_buffer[i_head] = val;
        q.push_front(val);
        BOOST_CHECK(q.back() == ring_buffer[i_tail]);
        BOOST_CHECK(q.front() == ring_buffer[i_head]);
        i_tail = (i_tail + 1) % len;
        i_head = (i_head + 1) % len;
        i_sample++;
    }
}

} // namespace dvbs2rx
} // namespace gr
