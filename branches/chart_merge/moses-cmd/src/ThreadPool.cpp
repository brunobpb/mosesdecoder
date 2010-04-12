// $Id$

/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2009 University of Edinburgh

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
***********************************************************************/


#include "ThreadPool.h"

using namespace std;
using namespace Moses;

Moses::ThreadPool::ThreadPool( size_t numThreads ) 
    : m_stopped(false), m_stopping(false)
{
    for (size_t i = 0; i < numThreads; ++i) {
        m_threads.create_thread(boost::bind(&ThreadPool::Execute,this));
    }
}

void Moses::ThreadPool::Execute()
{
    do {
        Task* task = NULL;
        { // Find a job to perform
            boost::mutex::scoped_lock lock(m_mutex);
            if (m_tasks.empty() && !m_stopped) {
                m_threadNeeded.wait(lock);
            }
            if (!m_stopped && !m_tasks.empty()) {
                task = m_tasks.front();
                m_tasks.pop();
            }
        }
        //Execute job
        if (task) {
            task->Run();
            delete task;
        }
        m_threadAvailable.notify_all();
    } while (!m_stopped);
#if defined(BOOST_HAS_PTHREADS)
    TRACE_ERR("Thread " << (int)pthread_self() << " exiting" << endl);
#endif
}

void Moses::ThreadPool::Submit( Task* task )
{
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_stopping) {
        throw runtime_error("ThreadPool stopping - unable to accept new jobs");
    }
    m_tasks.push(task);
    m_threadNeeded.notify_all();
     
}

void Moses::ThreadPool::Stop(bool processRemainingJobs)
{
    {
        //prevent more jobs from being added to the queue
        boost::mutex::scoped_lock lock(m_mutex);
        if (m_stopped) return;
        m_stopping = true;
    }
    if (processRemainingJobs) {
        boost::mutex::scoped_lock lock(m_mutex);
        //wait for queue to drain.
        while (!m_tasks.empty() && !m_stopped) {
            m_threadAvailable.wait(lock);
        }
    }
    //tell all threads to stop
    {
        boost::mutex::scoped_lock lock(m_mutex);
        m_stopped = true;
    }
    m_threadNeeded.notify_all();
    
    cerr << m_threads.size() << endl;
    m_threads.join_all();
}