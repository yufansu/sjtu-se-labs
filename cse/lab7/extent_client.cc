// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <vector>
#include <fstream>

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
  transnum = 0;
}

// a demo to show how to use RPC
extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::getattr, eid, attr);
  return ret;
}

extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id, unsigned int mode, unsigned short uid, unsigned short gid)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  // extent_t et = {mode, uid, gid};

  ret = cl->call(extent_protocol::create, type, mode, uid, gid, id);
  std::stringstream ss;
  ss << transnum << " create " <<id;
  std::string nlog;
  ss >> nlog;
  nlog = ss.str();
  log(nlog);
  return ret;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  ret = cl->call(extent_protocol::get, eid, buf);
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  // ret = cl->call(extent_protocol::put, eid, buf, ret);

  std::stringstream ss;
  ss << transnum << " put " << eid << " " << buf.size() << ' ' << buf;
  std::string nlog;
  nlog = ss.str();
  ret = log(nlog);
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  // ret = cl->call(extent_protocol::remove, eid, ret);

  std::stringstream ss;
  ss << transnum << " remove " << eid;
  std::string nlog;
  nlog = ss.str();
  ret = log(nlog);
  return ret;
}

extent_protocol::status
extent_client::change_mode(extent_protocol::extentid_t eid, unsigned int mode)
{
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::change_mode, eid, mode, ret);
  return ret;
}

extent_protocol::status
extent_client::change_owner(extent_protocol::extentid_t eid, unsigned short uid, unsigned short gid)
{
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::change_owner, eid, uid, gid, ret);
  return ret;
}

extent_protocol::status
extent_client::log(std::string log)
{
  extent_protocol::status ret = extent_protocol::OK;
  if (log == "begin"){
    ret = cl->call(extent_protocol::log, log, transnum);
    return ret;
  }
  else if (log == "end"){
    std::stringstream ss;
    ss << transnum << ' ' << log;
    log = ss.str();
  }

  int n;
  ret = cl->call(extent_protocol::log, log, n);
  return ret;
}

extent_protocol::status extent_client::commit()
{
  extent_protocol::status ret = extent_protocol::OK;
  int i, j;
  cl->call(extent_protocol::commit, i, j);
  return ret;
}

extent_protocol::status extent_client::undo()
{
  extent_protocol::status ret = extent_protocol::OK;
  int i, j;
  cl->call(extent_protocol::undo, i, j);
  return ret;
}

extent_protocol::status extent_client::redo()
{
  extent_protocol::status ret = extent_protocol::OK;
  int i, j;
  cl->call(extent_protocol::redo, i, j);
  return ret;
}
