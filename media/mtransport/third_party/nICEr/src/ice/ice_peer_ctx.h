/*
Copyright (c) 2007, Adobe Systems, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/



#ifndef _ice_peer_ctx_h
#define _ice_peer_ctx_h
#ifdef __cplusplus
using namespace std;
extern "C" {
#endif /* __cplusplus */

struct nr_ice_peer_ctx_ {
  char *label;
  nr_ice_ctx *ctx;
  nr_ice_handler *handler;

  UCHAR controlling; /* 1 for controlling, 0 for controlled */
  UINT8 tiebreaker;

  char *peer_ufrag;
  char *peer_pwd;
  int peer_lite;
  int peer_ice_mismatch;

  nr_ice_media_stream_head peer_streams;
  int active_streams;
  int waiting_pairs;

  void *done_cb_timer;
  UCHAR reported_done;

  STAILQ_ENTRY(nr_ice_peer_ctx_) entry;
};

typedef STAILQ_HEAD(nr_ice_peer_ctx_head_, nr_ice_peer_ctx_) nr_ice_peer_ctx_head;

int nr_ice_peer_ctx_create(nr_ice_ctx *ctx, nr_ice_handler *handler,char *label, nr_ice_peer_ctx **pctxp);
int nr_ice_peer_ctx_destroy(nr_ice_peer_ctx **pctxp);
int nr_ice_peer_ctx_parse_stream_attributes(nr_ice_peer_ctx *pctx, nr_ice_media_stream *stream, char **attrs, int attr_ct);
int nr_ice_peer_ctx_parse_trickle_candidate(nr_ice_peer_ctx *pctx, nr_ice_media_stream *stream, char *cand);

int nr_ice_peer_ctx_pair_candidates(nr_ice_peer_ctx *pctx);
int nr_ice_peer_ctx_parse_global_attributes(nr_ice_peer_ctx *pctx, char **attrs, int attr_ct);
int nr_ice_peer_ctx_start_checks(nr_ice_peer_ctx *pctx);
int nr_ice_peer_ctx_start_checks2(nr_ice_peer_ctx *pctx, int allow_non_first);
int nr_ice_peer_ctx_dump_state(nr_ice_peer_ctx *pctx,FILE *out);
int nr_ice_peer_ctx_log_state(nr_ice_peer_ctx *pctx);
int nr_ice_peer_ctx_stream_done(nr_ice_peer_ctx *pctx, nr_ice_media_stream *stream);
int nr_ice_peer_ctx_find_component(nr_ice_peer_ctx *pctx, nr_ice_media_stream *str, int component_id, nr_ice_component **compp);
int nr_ice_peer_ctx_deliver_packet_maybe(nr_ice_peer_ctx *pctx, nr_ice_component *comp, nr_transport_addr *source_addr, UCHAR *data, int len);
int nr_ice_peer_ctx_disable_component(nr_ice_peer_ctx *pctx, nr_ice_media_stream *lstream, int component_id);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif

