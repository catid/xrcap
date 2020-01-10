# Apps

These are network-aware peer2peer applications.  The clients and servers find eachother via a centralized rendezvous server on a known host.

## capture_client

Dynamic library for reuse by other applications:
+ Receives stream from a `capture_server`.
+ Records stream to disk.
+ Replays streams from disk. <= Useful for end-user applications.

## capture_server

Reads cameras, compresses the video, and transmits it.

GUI application that connects to a `rendezvous_server`, allowing `viewer` and other applications to view streams from attached cameras.

Video data is protected by a password.

## rendezvous_server

Console application that `capture_client` and `capture_server` connect to.

This allows a single `capture_client` to receive time-synched video from multiple `capture_server` computers.

## viewer

GUI application that connects to the `rendezvous_server` and then connects to all `capture_servers` available.

## xrcap_csharp

C# wrapper around the `capture_client` API, allowing the client to be used from Unity on Windows.
