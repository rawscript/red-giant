/**
 * Basic RGTP Demo
 * 
 * This example demonstrates the basic functionality of the RGTP library:
 * - Creating an RGTP socket
 * - Binding to a port
 * - Basic exposer and puller functionality
 */

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>