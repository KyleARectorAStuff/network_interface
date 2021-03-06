/*
* Unpublished Copyright (c) 2009-2017 AutonomouStuff, LLC, All Rights Reserved.
*
* This file is part of the network_interface ROS 1.0 driver which is released under the MIT license.
* See file LICENSE included with this software or go to https://opensource.org/licenses/MIT for full license details.
*/

#include <network_interface.h>

using namespace std;
using namespace AS::Network;
using boost::asio::ip::tcp;

//Default constructor.
TCPInterface::TCPInterface() :
  socket_(io_service_)
{
}

//Default destructor.
TCPInterface::~TCPInterface()
{
}

return_statuses TCPInterface::open(const char *ip_address, const int &port)
{
  if (socket_.is_open())
    return OK;

  stringstream sPort;
  sPort << port;
  tcp::resolver res(io_service_);
  tcp::resolver::query query(tcp::v4(), ip_address, sPort.str());
  tcp::resolver::iterator it = res.resolve(query);
  boost::system::error_code ec;

  socket_.connect(*it, ec);

  if (ec.value() == boost::system::errc::success)
  {
    return OK;
  }
  else if (ec.value() == boost::asio::error::invalid_argument)
  {
    return BAD_PARAM;
  }
  else
  {
    close();
    return INIT_FAILED;
  }
}

return_statuses TCPInterface::close()
{
  if (!socket_.is_open())
    return OK;

  boost::system::error_code ec;
  socket_.close(ec);

  if (ec.value() == boost::system::errc::success)
  {
    return OK;
  }
  else
  {
    return CLOSE_FAILED;
  }
}

bool TCPInterface::is_open()
{
  return socket_.is_open();
}

// Socket timeout handler
void TCPInterface::timeout_handler(const boost::system::error_code& error) 
{ 
  // Set the timeout flag and set the error code appropriately
  timeout_triggered_ = true;
  error_.assign(boost::system::errc::timed_out, boost::system::system_category());
} 

// Socket read handler
void TCPInterface::read_handler(const boost::system::error_code& error, size_t bytes_read)
{
  // If the operation was not aborted, store the bytes that were read and set the read flag
  if (error != boost::asio::error::operation_aborted)
    message_received_ = true;
    bytes_read_ = bytes_read;
}

return_statuses TCPInterface::read(unsigned char *msg,
                                   const size_t &buf_size,
                                   size_t &bytes_read)
{
  // Validate that the socket is connected
  if (!socket_.is_open())
    return SOCKET_CLOSED;

  // Set timemout and read flags
  timeout_triggered_ = false;
  message_received_ = false;

  error_.assign(boost::system::errc::success, boost::system::system_category());

  // Set a timer to run asychronously
  boost::asio::deadline_timer timer(io_service_,
                                    boost::posix_time::milliseconds(10));
  timer.async_wait(boost::bind(&TCPInterface::timeout_handler,
                               this,
                               boost::asio::placeholders::error));
  // Read the socket asychronously
  boost::asio::async_read(socket_,
                          boost::asio::buffer(msg, buf_size),
                          boost::bind(&TCPInterface::read_handler,
                                      this,
                                      boost::asio::placeholders::error,
                                      boost::asio::placeholders::bytes_transferred));
  // Run until a handler is called
  while (io_service_.run_one())
  {
    // If there is a callback with a received message, store the read bytes and cancel the timeout timer
    if (message_received_)
    {
      timer.cancel();
      bytes_read = bytes_read_;
    }
    // If the timeout is reached, cancel the socket operation
    else if (timeout_triggered_)
    {
      socket_.cancel();
    }
  }
  io_service_.reset();

  // Return the correct error code
  if (error_.value() == boost::system::errc::success)
  {
    return OK;
  }
  else if (error_.value() == boost::system::errc::timed_out)
  {
    return SOCKET_TIMEOUT;
  }
  else
  {
    return READ_FAILED;
  }
}

return_statuses TCPInterface::read_exactly(unsigned char *msg,
                                           const size_t &buf_size,
                                           const size_t &bytes_to_read)
{
  if (!socket_.is_open())
    return SOCKET_CLOSED;

  boost::system::error_code ec;
  boost::asio::read(socket_, boost::asio::buffer(msg, buf_size), boost::asio::transfer_exactly(bytes_to_read), ec);

  if (ec.value() == boost::system::errc::success)
  {
    return OK;
  }
  else
  {
    return READ_FAILED;
  }
}

return_statuses TCPInterface::write(unsigned char *msg, const size_t &msg_size)
{
  if (!socket_.is_open())
    return SOCKET_CLOSED;

  boost::system::error_code ec;
  boost::asio::write(socket_, boost::asio::buffer(msg, msg_size), ec);

  if (ec.value() == boost::system::errc::success)
  {
    return OK;
  }
  else
  {
    return WRITE_FAILED;
  }
}
