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

#pragma once

#include <condition_variable>
#include <mutex>
#include <vector>

/// a generic message, base class
class message
{
  public:
    using ptr = std::unique_ptr<message>;

  private:
    friend class message_queue;
    bool needsanswer;
    mutable bool result;

    // no copy, no move
    message(const message&)            = delete;
    message& operator=(const message&) = delete;
    message(message&&)                 = delete;
    message& operator=(message&&)      = delete;

  protected:
    /// evaluate the message. overload with your functionality.
    ///@note Failures should be reported with exceptions.
    virtual void eval() const = 0;

  public:
    /// create message
    message() = default;

    /// destroy message
    virtual ~message() = default;

    /// evaluate the message. Do not overload!
    ///@note Method is <b>not</b> virtual by intent. Do <b>not</b> overload. use
    /// eval() instead.
    void evaluate() const;

    /// get result of evaluation
    bool get_result() const { return result; }
};

/// an C++ message queue with generic messages
class message_queue
{
  private:
    // no copy
    message_queue(const message_queue&)            = delete;
    message_queue& operator=(const message_queue&) = delete;

  protected:
    std::vector<message::ptr> myqueue;
    std::mutex mymutex;
    std::condition_variable emptycondvar;
    std::condition_variable ackcondvar;
    bool msginqueue{false};
    bool abortwait{false};              // set to true by wakeup_receiver()
    std::vector<message::ptr> ackqueue; // queue with acknowledged messages

  public:
    /// create message queue
    message_queue();

    /// destroy message queue
    ~message_queue();

    /// send a message
    ///@param msg - message to send
    ///@param waitforanswer - true to send message synchronously (wait for reply
    /// with result)
    ///@return result of answer or true when message is asynchronous
    bool send(message::ptr msg, bool waitforanswer = true);

    /// wakeup thread waiting for a message
    void wakeup_receiver();

    /// wait for a messages
    ///@param wait - if true block while queue is empty, if false only test and
    /// do not block
    ///@return list of messages
    std::vector<message::ptr> receive(bool wait = true);

    /// acknowledge received message.
    ///@note Must be called for every message passed from the receive function!
    void acknowledge(message::ptr msg);

    /// process all messages, that is wait for messages, run eval() for every
    /// message and ack them
    ///@param wait - true: block if queue is empty
    void process_messages(bool wait = true);
};
