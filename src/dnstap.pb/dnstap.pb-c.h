/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: dnstap.proto */

#ifndef PROTOBUF_C_dnstap_2eproto__INCLUDED
#define PROTOBUF_C_dnstap_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1000000
# error This file was generated by a newer version of protoc-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1002001 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protoc-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protoc-c.
#endif


typedef struct _Dnstap__Dnstap Dnstap__Dnstap;
typedef struct _Dnstap__Message Dnstap__Message;


/* --- enums --- */

/*
 * Identifies which field below is filled in.
 */
typedef enum _Dnstap__Dnstap__Type {
  DNSTAP__DNSTAP__TYPE__MESSAGE = 1
    PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(DNSTAP__DNSTAP__TYPE)
} Dnstap__Dnstap__Type;
typedef enum _Dnstap__Message__Type {
  /*
   * AUTH_QUERY is a DNS query message received from a resolver by an
   * authoritative name server, from the perspective of the authorative
   * name server.
   */
  DNSTAP__MESSAGE__TYPE__AUTH_QUERY = 1,
  /*
   * AUTH_RESPONSE is a DNS response message sent from an authoritative
   * name server to a resolver, from the perspective of the authoritative
   * name server.
   */
  DNSTAP__MESSAGE__TYPE__AUTH_RESPONSE = 2,
  /*
   * RESOLVER_QUERY is a DNS query message sent from a resolver to an
   * authoritative name server, from the perspective of the resolver.
   * Resolvers typically clear the RD (recursion desired) bit when
   * sending queries.
   */
  DNSTAP__MESSAGE__TYPE__RESOLVER_QUERY = 3,
  /*
   * RESOLVER_RESPONSE is a DNS response message received from an
   * authoritative name server by a resolver, from the perspective of
   * the resolver.
   */
  DNSTAP__MESSAGE__TYPE__RESOLVER_RESPONSE = 4,
  /*
   * CLIENT_QUERY is a DNS query message sent from a client to a DNS
   * server which is expected to perform further recursion, from the
   * perspective of the DNS server. The client may be a stub resolver or
   * forwarder or some other type of software which typically sets the RD
   * (recursion desired) bit when querying the DNS server. The DNS server
   * may be a simple forwarding proxy or it may be a full recursive
   * resolver.
   */
  DNSTAP__MESSAGE__TYPE__CLIENT_QUERY = 5,
  /*
   * CLIENT_RESPONSE is a DNS response message sent from a DNS server to
   * a client, from the perspective of the DNS server. The DNS server
   * typically sets the RA (recursion available) bit when responding.
   */
  DNSTAP__MESSAGE__TYPE__CLIENT_RESPONSE = 6,
  /*
   * FORWARDER_QUERY is a DNS query message sent from a downstream DNS
   * server to an upstream DNS server which is expected to perform
   * further recursion, from the perspective of the downstream DNS
   * server.
   */
  DNSTAP__MESSAGE__TYPE__FORWARDER_QUERY = 7,
  /*
   * FORWARDER_RESPONSE is a DNS response message sent from an upstream
   * DNS server performing recursion to a downstream DNS server, from the
   * perspective of the downstream DNS server.
   */
  DNSTAP__MESSAGE__TYPE__FORWARDER_RESPONSE = 8,
  /*
   * STUB_QUERY is a DNS query message sent from a stub resolver to a DNS
   * server, from the perspective of the stub resolver.
   */
  DNSTAP__MESSAGE__TYPE__STUB_QUERY = 9,
  /*
   * STUB_RESPONSE is a DNS response message sent from a DNS server to a
   * stub resolver, from the perspective of the stub resolver.
   */
  DNSTAP__MESSAGE__TYPE__STUB_RESPONSE = 10,
  /*
   * TOOL_QUERY is a DNS query message sent from a DNS software tool to a
   * DNS server, from the perspective of the tool.
   */
  DNSTAP__MESSAGE__TYPE__TOOL_QUERY = 11,
  /*
   * TOOL_RESPONSE is a DNS response message received by a DNS software
   * tool from a DNS server, from the perspective of the tool.
   */
  DNSTAP__MESSAGE__TYPE__TOOL_RESPONSE = 12
    PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(DNSTAP__MESSAGE__TYPE)
} Dnstap__Message__Type;
/*
 * SocketFamily: the network protocol family of a socket. This specifies how
 * to interpret "network address" fields.
 */
typedef enum _Dnstap__SocketFamily {
  /*
   * IPv4 (RFC 791)
   */
  DNSTAP__SOCKET_FAMILY__INET = 1,
  /*
   * IPv6 (RFC 2460)
   */
  DNSTAP__SOCKET_FAMILY__INET6 = 2
    PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(DNSTAP__SOCKET_FAMILY)
} Dnstap__SocketFamily;
/*
 * SocketProtocol: the transport protocol of a socket. This specifies how to
 * interpret "transport port" fields.
 */
typedef enum _Dnstap__SocketProtocol {
  /*
   * User Datagram Protocol (RFC 768)
   */
  DNSTAP__SOCKET_PROTOCOL__UDP = 1,
  /*
   * Transmission Control Protocol (RFC 793)
   */
  DNSTAP__SOCKET_PROTOCOL__TCP = 2
    PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(DNSTAP__SOCKET_PROTOCOL)
} Dnstap__SocketProtocol;

/* --- messages --- */

/*
 * "Dnstap": this is the top-level dnstap type, which is a "union" type that
 * contains other kinds of dnstap payloads, although currently only one type
 * of dnstap payload is defined.
 * See: https://developers.google.com/protocol-buffers/docs/techniques#union
 */
struct  _Dnstap__Dnstap
{
  ProtobufCMessage base;
  /*
   * DNS server identity.
   * If enabled, this is the identity string of the DNS server which generated
   * this message. Typically this would be the same string as returned by an
   * "NSID" (RFC 5001) query.
   */
  protobuf_c_boolean has_identity;
  ProtobufCBinaryData identity;
  /*
   * DNS server version.
   * If enabled, this is the version string of the DNS server which generated
   * this message. Typically this would be the same string as returned by a
   * "version.bind" query.
   */
  protobuf_c_boolean has_version;
  ProtobufCBinaryData version;
  /*
   * Extra data for this payload.
   * This field can be used for adding an arbitrary byte-string annotation to
   * the payload. No encoding or interpretation is applied or enforced.
   */
  protobuf_c_boolean has_extra;
  ProtobufCBinaryData extra;
  Dnstap__Dnstap__Type type;
  /*
   * One of the following will be filled in.
   */
  Dnstap__Message *message;
};
#define DNSTAP__DNSTAP__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&dnstap__dnstap__descriptor) \
    , 0,{0,NULL}, 0,{0,NULL}, 0,{0,NULL}, 0, NULL }


/*
 * Message: a wire-format (RFC 1035 section 4) DNS message and associated
 * metadata. Applications generating "Message" payloads should follow
 * certain requirements based on the MessageType, see below.
 */
struct  _Dnstap__Message
{
  ProtobufCMessage base;
  /*
   * One of the Type values described above.
   */
  Dnstap__Message__Type type;
  /*
   * One of the SocketFamily values described above.
   */
  protobuf_c_boolean has_socket_family;
  Dnstap__SocketFamily socket_family;
  /*
   * One of the SocketProtocol values described above.
   */
  protobuf_c_boolean has_socket_protocol;
  Dnstap__SocketProtocol socket_protocol;
  /*
   * The network address of the message initiator.
   * For SocketFamily INET, this field is 4 octets (IPv4 address).
   * For SocketFamily INET6, this field is 16 octets (IPv6 address).
   */
  protobuf_c_boolean has_query_address;
  ProtobufCBinaryData query_address;
  /*
   * The network address of the message responder.
   * For SocketFamily INET, this field is 4 octets (IPv4 address).
   * For SocketFamily INET6, this field is 16 octets (IPv6 address).
   */
  protobuf_c_boolean has_response_address;
  ProtobufCBinaryData response_address;
  /*
   * The transport port of the message initiator.
   * This is a 16-bit UDP or TCP port number, depending on SocketProtocol.
   */
  protobuf_c_boolean has_query_port;
  uint32_t query_port;
  /*
   * The transport port of the message responder.
   * This is a 16-bit UDP or TCP port number, depending on SocketProtocol.
   */
  protobuf_c_boolean has_response_port;
  uint32_t response_port;
  /*
   * The time at which the DNS query message was sent or received, depending
   * on whether this is an AUTH_QUERY, RESOLVER_QUERY, or CLIENT_QUERY.
   * This is the number of seconds since the UNIX epoch.
   */
  protobuf_c_boolean has_query_time_sec;
  uint64_t query_time_sec;
  /*
   * The time at which the DNS query message was sent or received.
   * This is the seconds fraction, expressed as a count of nanoseconds.
   */
  protobuf_c_boolean has_query_time_nsec;
  uint32_t query_time_nsec;
  /*
   * The initiator's original wire-format DNS query message, verbatim.
   */
  protobuf_c_boolean has_query_message;
  ProtobufCBinaryData query_message;
  /*
   * The "zone" or "bailiwick" pertaining to the DNS query message.
   * This is a wire-format DNS domain name.
   */
  protobuf_c_boolean has_query_zone;
  ProtobufCBinaryData query_zone;
  /*
   * The time at which the DNS response message was sent or received,
   * depending on whether this is an AUTH_RESPONSE, RESOLVER_RESPONSE, or
   * CLIENT_RESPONSE.
   * This is the number of seconds since the UNIX epoch.
   */
  protobuf_c_boolean has_response_time_sec;
  uint64_t response_time_sec;
  /*
   * The time at which the DNS response message was sent or received.
   * This is the seconds fraction, expressed as a count of nanoseconds.
   */
  protobuf_c_boolean has_response_time_nsec;
  uint32_t response_time_nsec;
  /*
   * The responder's original wire-format DNS response message, verbatim.
   */
  protobuf_c_boolean has_response_message;
  ProtobufCBinaryData response_message;
};
#define DNSTAP__MESSAGE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&dnstap__message__descriptor) \
    , 0, 0,0, 0,0, 0,{0,NULL}, 0,{0,NULL}, 0,0, 0,0, 0,0, 0,0, 0,{0,NULL}, 0,{0,NULL}, 0,0, 0,0, 0,{0,NULL} }


/* Dnstap__Dnstap methods */
void   dnstap__dnstap__init
                     (Dnstap__Dnstap         *message);
size_t dnstap__dnstap__get_packed_size
                     (const Dnstap__Dnstap   *message);
size_t dnstap__dnstap__pack
                     (const Dnstap__Dnstap   *message,
                      uint8_t             *out);
size_t dnstap__dnstap__pack_to_buffer
                     (const Dnstap__Dnstap   *message,
                      ProtobufCBuffer     *buffer);
Dnstap__Dnstap *
       dnstap__dnstap__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   dnstap__dnstap__free_unpacked
                     (Dnstap__Dnstap *message,
                      ProtobufCAllocator *allocator);
/* Dnstap__Message methods */
void   dnstap__message__init
                     (Dnstap__Message         *message);
size_t dnstap__message__get_packed_size
                     (const Dnstap__Message   *message);
size_t dnstap__message__pack
                     (const Dnstap__Message   *message,
                      uint8_t             *out);
size_t dnstap__message__pack_to_buffer
                     (const Dnstap__Message   *message,
                      ProtobufCBuffer     *buffer);
Dnstap__Message *
       dnstap__message__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   dnstap__message__free_unpacked
                     (Dnstap__Message *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*Dnstap__Dnstap_Closure)
                 (const Dnstap__Dnstap *message,
                  void *closure_data);
typedef void (*Dnstap__Message_Closure)
                 (const Dnstap__Message *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCEnumDescriptor    dnstap__socket_family__descriptor;
extern const ProtobufCEnumDescriptor    dnstap__socket_protocol__descriptor;
extern const ProtobufCMessageDescriptor dnstap__dnstap__descriptor;
extern const ProtobufCEnumDescriptor    dnstap__dnstap__type__descriptor;
extern const ProtobufCMessageDescriptor dnstap__message__descriptor;
extern const ProtobufCEnumDescriptor    dnstap__message__type__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_dnstap_2eproto__INCLUDED */