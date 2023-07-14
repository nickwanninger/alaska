#include <alaska.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>

// to get memcached working...
ssize_t sendmsg(int fd, const struct msghdr *msg, int flags) {
  static __typeof(sendmsg) *real_sendmsg = NULL;
  if (real_sendmsg == NULL) {
    real_sendmsg = dlsym(RTLD_NEXT, "sendmsg");
		assert(real_sendmsg != sendmsg);
  }
  // printf("real send msg = %p\n", real_sendmsg);

  // copy the message
  struct msghdr fixed_msg = *msg;
  // Leak/translate the name and control
  fixed_msg.msg_name = __alaska_leak(fixed_msg.msg_name);
  fixed_msg.msg_control = __alaska_leak(fixed_msg.msg_control);
  // DANGER: allocate vecs
  struct iovec vecs[msg->msg_iovlen];
  // Use those vecs from the stack
  fixed_msg.msg_iov = vecs;
  // Leak/translate each vec base address
  for (size_t i = 0; i < msg->msg_iovlen; i++) {
    vecs[i] = msg->msg_iov[i];
    vecs[i].iov_base = __alaska_leak(vecs[i].iov_base);
  }

  return real_sendmsg(fd, &fixed_msg, flags);
}


// to get redis working...
ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
  static __typeof(writev) *real = NULL;
  if (real == NULL) {
    real = dlsym(RTLD_NEXT, "writev");
    assert(real != writev);
  }
  // DANGER: allocate vecs
  struct iovec vecs[iovcnt];
  // Leak/translate each vec base address
  for (int i = 0; i < iovcnt; i++) {
    vecs[i] = iov[i];
    vecs[i].iov_base = __alaska_leak(vecs[i].iov_base);
  }

  return real(fd, vecs, iovcnt);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
  static __typeof(readv) *real = NULL;
  if (real == NULL) {
    real = dlsym(RTLD_NEXT, "readv");
    assert(real != readv);
  }
  // DANGER: allocate vecs
  struct iovec vecs[iovcnt];
  // Leak/translate each vec base address
  for (int i = 0; i < iovcnt; i++) {
    vecs[i] = iov[i];
    vecs[i].iov_base = __alaska_leak(vecs[i].iov_base);
  }

  return real(fd, vecs, iovcnt);
}
