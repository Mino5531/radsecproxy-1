/* RADIUS client doing blocking i/o.  */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <event2/event.h>
#include <freeradius/libradius.h>
#include <radsec/radsec.h>
#include <radsec/request.h>
#include "debug.h"		/* For rs_dump_packet().  */

#define SECRET "sikrit"
#define USER_NAME "molgan@PROJECT-MOONSHOT.ORG"
#define USER_PW "password"

struct rs_error *
blocking_client (const char *av1, const char *av2, int use_request_object_flag)
{
  struct rs_context *h = NULL;
  struct rs_connection *conn = NULL;
  struct rs_request *request = NULL;
  struct rs_packet *req = NULL, *resp = NULL;
  struct rs_error *err = NULL;

  if (rs_context_create (&h))
    return NULL;

#if !defined (USE_CONFIG_FILE)
  {
    struct rs_peer *server;

    if (rs_context_init_freeradius_dict (h, "/usr/share/freeradius/dictionary"))
      goto cleanup;
    if (rs_conn_create (h, &conn, NULL))
      goto cleanup;
    rs_conn_set_type (conn, RS_CONN_TYPE_UDP);
    if (rs_peer_create (conn, &server))
      goto cleanup;
    if (rs_peer_set_address (server, av1, av2))
      goto cleanup;
    rs_peer_set_timeout (server, 1);
    rs_peer_set_retries (server, 3);
    if (rs_peer_set_secret (server, SECRET))
      goto cleanup;
  }
#else  /* defined (USE_CONFIG_FILE) */
  if (rs_context_read_config (h, av1))
    goto cleanup;
  if (rs_context_init_freeradius_dict (h, NULL))
    goto cleanup;
  if (rs_conn_create (h, &conn, av2))
    goto cleanup;
#endif	/* defined (USE_CONFIG_FILE) */

  if (use_request_object_flag)
    {
      if (rs_request_create_authn (conn, &request, USER_NAME, USER_PW))
	goto cleanup;
      if (rs_request_send (request, &resp))
	goto cleanup;
    }
  else
    {
      if (rs_packet_create_authn_request (conn, &req, USER_NAME, USER_PW))
	goto cleanup;
      if (rs_packet_send (req, NULL))
	goto cleanup;
      if (rs_conn_receive_packet (conn, req, &resp))
	goto cleanup;
    }

  if (resp)
    {
      rs_dump_packet (resp);
      if (rs_packet_frpkt (resp)->code == PW_AUTHENTICATION_ACK)
	printf ("Good auth.\n");
      else
	printf ("Bad auth: %d\n", rs_packet_frpkt (resp)->code);
    }
  else
    fprintf (stderr, "%s: no response\n", __func__);

 cleanup:
  err = rs_err_ctx_pop (h);
  if (err == RSE_OK)
    err = rs_err_conn_pop (conn);
  if (resp)
    rs_packet_destroy (resp);
  if (request)
    rs_request_destroy (request);
  if (conn)
    rs_conn_destroy (conn);
  if (h)
    rs_context_destroy (h);

  return err;
}

int
main (int argc, char *argv[])
{
  int use_request_object_flag = 0;
  struct rs_error *err;

  if (argc > 1 && argv[1] && argv[1][0] == '-' && argv[1][1] == 'r')
    {
      use_request_object_flag = 1;
      argc--;
      argv++;
    }
  err = blocking_client (argv[1], argv[2], use_request_object_flag);
  if (err)
    {
      fprintf (stderr, "%s\n", rs_err_msg (err));
      return rs_err_code (err, 1);
    }
  return 0;
}
