/*
Danger from the Deep - Open source submarine simulation
Copyright (C) 2003-2020  Thorsten Jordan, Luis Barrancos and others.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// multithreading primitives: messaging, message queue
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "message_queue.hpp"

#include "error.hpp"
#include "log.hpp"

void message::evaluate() const
{
    result = false;
    try {
        eval();
        result = true;
    }
    catch (std::exception& /*e*/) {
        // avoid to spam the log. define when needed.
        // log_debug("msg eval failed: " << e.what());
    }
}

message_queue::message_queue() = default;

message_queue::~message_queue()
{
    // Inform receiver
    wakeup_receiver();

    // clean up queues, report all pending messages as failed, then wait
    // until ackqueue is empty... it should not happen though...
    bool ackqueueempty = true;
    {
        std::unique_lock<std::mutex> ml(mymutex);
        for (auto& elem : myqueue) {
            if (elem->needsanswer) {
                ackqueue.push_back(std::move(elem));
            }
        }
        ackcondvar.notify_all();
        ackqueueempty = ackqueue.empty();
    }

    while (!ackqueueempty) {
        // fixme// thread::sleep(10);//fixme use sys! uses SDL_Delay...is there
        // a c++ solution?!
        std::unique_lock<std::mutex> ml(mymutex);
        ackqueueempty = ackqueue.empty();
    }
}

auto message_queue::send(message::ptr msg, bool waitforanswer) -> bool
{
    msg->needsanswer  = waitforanswer;
    msg->result       = false;
    message* msg_addr = msg.get(); // address only for comparison
    std::unique_lock<std::mutex> oml(mymutex);

    bool e = myqueue.empty();
    myqueue.push_back(std::move(msg));
    msginqueue = true;

    if (e) {
        emptycondvar.notify_all();
    }

    if (waitforanswer) {
        while (true) {
            ackcondvar.wait(oml);
            // check if this message has been acknowledged
            for (auto it = ackqueue.begin(); it != ackqueue.end();) {
                if (it->get() == msg_addr) {
                    // found it, return result, delete and unqueue message
                    bool result = (*it)->result;
                    ackqueue.erase(it);
                    return result;
                }
            }
        }
    }
    return true;
}

void message_queue::wakeup_receiver()
{
    // set a special flag to avoid another thread to enter the wait() command
    // if this signal comes while the other thread tests wether to enter wait
    // state
    std::unique_lock<std::mutex> oml(mymutex);
    abortwait = true;
    emptycondvar.notify_all();
}

auto message_queue::receive(bool wait) -> std::vector<message::ptr>
{
    std::vector<message::ptr> result;
    std::unique_lock<std::mutex> oml(mymutex);

    if (myqueue.empty()) {
        if (wait && !abortwait) {
            emptycondvar.wait(oml);
        } else {
            // no need to wait, so clear abort signal
            abortwait  = false;
            msginqueue = false;
            return result;
        }
    }

    abortwait = false;

    // if we woke up and queue is still empty, we received a wakeup signal
    // so result either is empty or contains the full queue after this line
    myqueue.swap(result);

    // any way, there are now no messages in the queue. return messages or empty
    // list.
    msginqueue = false;
    return result;
}

void message_queue::acknowledge(message::ptr msg)
{
    if (msg.get() == nullptr) {
        THROW(error, "acknowledge without message called");
    }
    bool needsanswer = msg->needsanswer;
    if (needsanswer) {
        std::unique_lock<std::mutex> oml(mymutex);
        ackqueue.push_back(std::move(msg));
        ackcondvar.notify_all();
    }
}

void message_queue::process_messages(bool wait)
{
    auto msgs = receive(wait);
    for (auto& msg : msgs) {
        msg->evaluate();
        acknowledge(std::move(msg));
    }
}
