// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&cond, NULL);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab4 code goes here
  pthread_mutex_lock(&mutex);
  if (lock_map.find(lid) != lock_map.end()){
      if (lock_map[lid] == FREE){
          lock_map[lid] = LOCKED;
      }
      else {
          while (lock_map[lid] == LOCKED){
              pthread_cond_wait(&cond, &mutex);
          }
          lock_map[lid] = LOCKED;
      }
  }
  else {
      lock_map.insert(std::pair<lock_protocol::lockid_t, int>(lid, LOCKED));
  }
  nacquire++;
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab4 code goes here
  pthread_mutex_lock(&mutex);

  if (lock_map.find(lid) != lock_map.end()){
      if (lock_map[lid] == LOCKED){
          lock_map[lid] = FREE;
          nacquire--;
      }
      else {
          ret = lock_protocol::RETRY;
      }
  }
  else {
      ret = lock_protocol::NOENT;
  }

  pthread_mutex_unlock(&mutex);
  pthread_cond_signal(&cond);

  return ret;
}
