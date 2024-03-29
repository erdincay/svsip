/* $Id: simpleua.c 1120 2007-03-30 18:57:01Z bennylp $ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
 * Copyright (C) 2007-2009  Samuel Vinson <samuelv0304@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */


/**
 * simpleua.c
 *
 * This is a very simple SIP user agent complete with media. The user
 * agent should do a proper SDP negotiation and start RTP media once
 * SDP negotiation has completed.
 *
 * This program does not register to SIP server.
 *
 * Capabilities to be demonstrated here:
 *  - Basic call
 *  - UDP transport at port 5060 (hard coded)
 *  - RTP socket at port 4000 (hard coded)
 *  - proper SDP negotiation
 *  - PCMA/PCMU codec only.
 *  - Audio/media to sound device.
 *
 *
 * Usage:
 *  - To make outgoing call, start simpleua with the URL of remote
 *    destination to contact.
 *    E.g.:
 *	 simpleua sip:user@remote
 *
 *  - Incoming calls will automatically be answered with 180, then 200.
 *
 * This program does not disconnect call.
 *
 * This program will quit once it has completed a single call.
 */

/* Include all headers. */
#include <pjsip.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjsip_ua.h>
#include <pjsip_simple.h>
#include <pjlib-util.h>
#include <pjlib.h>

#include <nds.h>
#include <fat.h>
#include <dswifi9.h>
#include "arm9_wifi.h"

#include <sys/socket.h>
#include <netinet/in.h>

#define CALLING
#define FREEPHONIE
#define AUDIO
#define MICRO

/* For logging purpose. */
#define THIS_FILE   "simpleua.c"

#if 1
#   define TRACE_(x)    PJ_LOG(5,x)
#else
#   define TRACE_(x)
#endif

#include "util.h"

/*
 * Static variables.
 */

static pj_bool_t	     g_complete;    /* Quit flag.		*/
static pjsip_endpoint	    *g_endpt;	    /* SIP endpoint.		*/
static pj_caching_pool	     cp;	    /* Global pool factory.	*/

static pjmedia_endpt	    *g_med_endpt;   /* Media endpoint.		*/
static pjmedia_sock_info     g_med_skinfo;  /* Socket info for media	*/
static pjmedia_transport    *g_med_transport;/* Media stream transport	*/

/* Call variables: */
static pjsip_inv_session    *g_inv;	    /* Current invite session.	*/
static pjmedia_session	    *g_med_session; /* Call's media session.	*/
static pjmedia_snd_port	    *g_snd_player;  /* Call's sound player	*/
static pjmedia_snd_port	    *g_snd_rec;	    /* Call's sound recorder.	*/

static pj_oshandle_t         file; /* Log file */
/*
 * Prototypes:
 */

/* Callback to be called when SDP negotiation is done in the call: */
static void call_on_media_update( pjsip_inv_session *inv,
				  pj_status_t status);

/* Callback to be called when invite session's state has changed: */
static void call_on_state_changed( pjsip_inv_session *inv, 
				   pjsip_event *e);

/* Callback to be called when dialog has forked: */
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e);

/* Callback to be called to handle incoming requests outside dialogs: */
static pj_bool_t on_rx_request( pjsip_rx_data *rdata );




/* This is a PJSIP module to be registered by application to handle
 * incoming requests outside any dialogs/transactions. The main purpose
 * here is to handle incoming INVITE request message, where we will
 * create a dialog and INVITE session for it.
 */
static pjsip_module mod_simpleua =
{
    NULL, NULL,			    /* prev, next.		*/
    { "mod-simpleua", 12 },	    /* Name.			*/
    -1,				    /* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION, /* Priority			*/
    NULL,			    /* load()			*/
    NULL,			    /* start()			*/
    NULL,			    /* stop()			*/
    NULL,			    /* unload()			*/
    &on_rx_request,		    /* on_rx_request()		*/
    NULL,			    /* on_rx_response()		*/
    NULL,			    /* on_tx_request.		*/
    NULL,			    /* on_tx_response()		*/
    NULL,			    /* on_tsx_state()		*/
};

void log_write_file(int level, const char *buffer, int len)
{
    long l =(long)(len);
    PJ_CHECK_STACK();
    /* Copy to terminal/file. */
    fputs(buffer, stdout);
    fflush(stdout);

    pj_file_write(file, buffer, &l);
//    pj_file_flush(file);
}

/*
 * main()
 *
 * If called with argument, treat argument as SIP URL to be called.
 * Otherwise wait for incoming calls.
 */
int main(int argc, char *argv[])
{
    pj_status_t status;
    
    powerON(/*POWER_ALL*/POWER_LCD);
    // install the default exception handler
    defaultExceptionHandler();
//    initSystem();
    consoleDemoInit();
    irqInit();
irqEnable(IRQ_VBLANK);
    fatInitDefault();
    arm9_wifiInit();
    arm9_wifiAutoconnect();
    
    /* Must init PJLIB first: */
    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
//pj_log_set_level(6);   
    // Devrait surement intervenir apr�s pj_init !!!
    pj_file_open( NULL, "logsimple.txt", PJ_O_WRONLY, &file);
    pj_log_set_log_func(&log_write_file);
    
//pj_dump_config();
//pj_in_addr addr;
//pj_gethostip(&addr);
//PJ_LOG(3,(THIS_FILE, "ip address %s", pj_inet_ntoa(addr))); 


    /* Then init PJLIB-UTIL: */
    status = pjlib_util_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);


    /* Create global endpoint: */
    {
	const pj_str_t *hostname;
	const char *endpt_name;

	/* Endpoint MUST be assigned a globally unique name.
	 * The name will be used as the hostname in Warning header.
	 */

	/* For this implementation, we'll use hostname for simplicity */
	hostname = pj_gethostname();
	endpt_name = hostname->ptr;
PJ_LOG(3,(THIS_FILE, "hostname: %s", endpt_name));

	/* Create the endpoint: */

	status = pjsip_endpt_create(&cp.factory, endpt_name, 
				    &g_endpt);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }


    /* 
     * Add UDP transport, with hard-coded port 
     * Alternatively, application can use pjsip_udp_transport_attach() to
     * start UDP transport, if it already has an UDP socket (e.g. after it
     * resolves the address with STUN).
     */
    {
TRACE_((THIS_FILE, "Add UDP transport."));   
	pj_sockaddr_in addr;

	addr.sin_family = pj_AF_INET();
	addr.sin_addr.s_addr = 0;
	addr.sin_port = pj_htons(5060);

	status = pjsip_udp_transport_start( g_endpt, &addr, NULL, 1, NULL);
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to start UDP transport", status);
		pj_file_close(file);
	    return 1;
	}
    }


    /* 
     * Init transaction layer.
     * This will create/initialize transaction hash tables etc.
     */
TRACE_((THIS_FILE, "Init transaction layer."));
    status = pjsip_tsx_layer_init_module(g_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* 
     * Initialize UA layer module.
     * This will create/initialize dialog hash tables etc.
     */
TRACE_((THIS_FILE, "Initialize UA layer module."));  
    status = pjsip_ua_init_module( g_endpt, NULL );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* 
     * Init invite session module.
     * The invite session module initialization takes additional argument,
     * i.e. a structure containing callbacks to be called on specific
     * occurence of events.
     *
     * The on_state_changed and on_new_session callbacks are mandatory.
     * Application must supply the callback function.
     *
     * We use on_media_update() callback in this application to start
     * media transmission.
     */
    {
	pjsip_inv_callback inv_cb;

	/* Init the callback for INVITE session: */
	pj_bzero(&inv_cb, sizeof(inv_cb));
	inv_cb.on_state_changed = &call_on_state_changed;
	inv_cb.on_new_session = &call_on_forked;
	inv_cb.on_media_update = &call_on_media_update;

	/* Initialize invite session module:  */
	status = pjsip_inv_usage_init(g_endpt, &inv_cb);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }

    /* Initialize 100rel support */
    status = pjsip_100rel_init_module(g_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /*
     * Register our module to receive incoming requests.
     */
    status = pjsip_endpt_register_module( g_endpt, &mod_simpleua);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* 
     * Initialize media endpoint.
     * This will implicitly initialize PJMEDIA too.
     */
#if PJ_HAS_THREADS
    status = pjmedia_endpt_create(&cp.factory, NULL, 1, &g_med_endpt);
#else
    status = pjmedia_endpt_create(&cp.factory, 
				  pjsip_endpt_get_ioqueue(g_endpt), 
				  0, &g_med_endpt);
#endif
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* 
     * Add PCMA/PCMU codec to the media endpoint. 
     */
#if defined(PJMEDIA_HAS_G711_CODEC) && PJMEDIA_HAS_G711_CODEC!=0
    status = pjmedia_codec_g711_init(g_med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
#endif

    
    /* 
     * Create media transport used to send/receive RTP/RTCP socket.
     * One media transport is needed for each call. Application may
     * opt to re-use the same media transport for subsequent calls.
     */
TRACE_((THIS_FILE, "pjmedia_transport_udp_create."));            
    status = pjmedia_transport_udp_create(g_med_endpt, NULL, 4000, 0, 
					  &g_med_transport);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to create media transport", status);
	pj_file_close(file);
	return 1;
    }

    /* 
     * Get socket info (address, port) of the media transport. We will
     * need this info to create SDP (i.e. the address and port info in
     * the SDP).
     */
    {
	pjmedia_transport_udp_info udp_info;
TRACE_((THIS_FILE, "pjmedia_transport_udp_get_info."));
	pjmedia_transport_udp_get_info(g_med_transport, &udp_info);
	pj_memcpy(&g_med_skinfo, &udp_info.skinfo, 
		  sizeof(pjmedia_sock_info));
    }


    /*
     * If URL is specified, then make call immediately.
     */
#ifdef CALLING
    /*if (argc > 1)*/ {
//	char temp[80];
	/*pj_str_t dst_uri = pj_str(argv[1]);*/   
	pj_str_t dst_uri = pj_str("sip:number@registrar");

	pj_str_t local_uri;
	//pj_str_t local_contact = pj_str("toto");
	pjsip_dialog *dlg;
	pjmedia_sdp_session *local_sdp;
	pjsip_tx_data *tdata;

//	pj_ansi_sprintf(temp, "sip:simpleuac@%s", pjsip_endpt_name(g_endpt)->ptr);
//    local_uri = pj_str(temp);
	local_uri = pj_str("sip:number@registrar:5060");

	/* Create UAC dialog */
TRACE_((THIS_FILE, "pjsip_dlg_create_uac."));           
	status = pjsip_dlg_create_uac( pjsip_ua_instance(), 
				       &local_uri,  /* local URI */
				       NULL,	    /* local Contact */
				       &dst_uri,    /* remote URI */
				       &dst_uri,    /* remote target */
				       &dlg);	    /* dialog */
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to create UAC dialog", status);
		pj_file_close(file);
	    return 1;
	}

	/* If we expect the outgoing INVITE to be challenged, then we should
	 * put the credentials in the dialog here, with something like this:
	 *
	    {
		pjsip_cred_info	cred[1];

		cred[0].realm	  = pj_str("sip.server.realm");
		cred[0].username  = pj_str("theuser");
		cred[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
		cred[0].data      = pj_str("thepassword");

		pjsip_auth_clt_set_credentials( &dlg->auth_sess, 1, cred);
	    }
	 *
	 */
    
	{
		pjsip_cred_info	cred[1];

		cred[0].realm	  = pj_str("*");
//		cred[0].scheme    = pj_str("digest");
		cred[0].username  = pj_str("username");
		cred[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
		cred[0].data      = pj_str("pwd");

		pjsip_auth_clt_set_credentials( &dlg->auth_sess, 1, cred);
	}

    /* Get the SDP body to be put in the outgoing INVITE, by asking
     * media endpoint to create one for us. The SDP will contain all
     * codecs that have been registered to it (in this case, only
     * PCMA and PCMU), plus telephony event.
     */
TRACE_((THIS_FILE, "pjmedia_endpt_create_sdp."));      
    status = pjmedia_endpt_create_sdp( g_med_endpt,     /* the media endpt  */
                       dlg->pool,       /* pool.        */
                       1,           /* # of streams */
                       &g_med_skinfo,   /* RTP sock info    */
                       &local_sdp);     /* the SDP result   */
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);



    /* Create the INVITE session, and pass the SDP returned earlier
     * as the session's initial capability.
     */
TRACE_((THIS_FILE, "pjsip_inv_create_uac."));      
    status = pjsip_inv_create_uac( dlg, local_sdp, 0, &g_inv);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


	/* If we want the initial INVITE to travel to specific SIP proxies,
	 * then we should put the initial dialog's route set here. The final
	 * route set will be updated once a dialog has been established.
	 * To set the dialog's initial route set, we do it with something
	 * like this:
	 *
	    {
		pjsip_route_hdr route_set;
		pjsip_route_hdr *route;
		const pj_str_t hname = { "Route", 5 };
		char *uri = "sip:proxy.server;lr";

		pj_list_init(&route_set);

		route = pjsip_parse_hdr( dlg->pool, &hname, 
					 uri, strlen(uri),
					 NULL);
		PJ_ASSERT_RETURN(route != NULL, 1);
		pj_list_push_back(&route_set, route);

		pjsip_dlg_set_route_set(dlg, &route_set);
	    }
	 *
	 * Note that Route URI SHOULD have an ";lr" parameter!
	 */

	/* Create initial INVITE request.
	 * This INVITE request will contain a perfectly good request and 
	 * an SDP body as well.
	 */
TRACE_((THIS_FILE, "pjsip_inv_invite."));      
	status = pjsip_inv_invite(g_inv, &tdata);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);



	/* Send initial INVITE request. 
	 * From now on, the invite session's state will be reported to us
	 * via the invite session callbacks.
	 */
TRACE_((THIS_FILE, "pjsip_inv_send_msg."));
	status = pjsip_inv_send_msg(g_inv, tdata);
//pj_file_close(file);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    }
#else
	/* No URL to make call to */

	PJ_LOG(3,(THIS_FILE, "Ready to accept incoming calls..."));
#endif
TRACE_((THIS_FILE, "Loop."));
//    /* Loop until one call is completed */
//    for (;!g_complete;) {
//	pj_time_val timeout = {0, 10};
////PJ_LOG(3,(THIS_FILE, "pjsip_endpt_handle_events."));
//	pjsip_endpt_handle_events(g_endpt, &timeout);
//    }

g_complete = 0;
while(!g_complete) 
{
    pj_time_val timeout = {0, 10};
    swiWaitForVBlank();
    scanKeys();
    uint32 key = keysDown(); 
    if (key & KEY_A)
    {
        break;
    }
    pjsip_endpt_handle_events(g_endpt, &timeout);
}       
printf("bye bye\n");

	
	
    /* On exit, dump current memory usage: */
    dump_pool_usage(THIS_FILE, &cp);

pj_file_close(file);

    return 0;
}



/*
 * Callback when INVITE session state has changed.
 * This callback is registered when the invite session module is initialized.
 * We mostly want to know when the invite session has been disconnected,
 * so that we can quit the application.
 */
static void call_on_state_changed( pjsip_inv_session *inv, 
				   pjsip_event *e)
{
    PJ_UNUSED_ARG(e);

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {

	PJ_LOG(3,(THIS_FILE, "Call DISCONNECTED [reason=%d (%s)]", 
		  inv->cause,
		  pjsip_get_status_text(inv->cause)->ptr));

	PJ_LOG(3,(THIS_FILE, "One call completed, application quitting..."));
	g_complete = 1;

    } else {

	PJ_LOG(3,(THIS_FILE, "Call state changed to %s", 
		  pjsip_inv_state_name(inv->state)));

    }
}


/* This callback is called when dialog has forked. */
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e)
{
    /* To be done... */
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);
}


/*
 * Callback when incoming requests outside any transactions and any
 * dialogs are received. We're only interested to hande incoming INVITE
 * request, and we'll reject any other requests with 500 response.
 */
static pj_bool_t on_rx_request( pjsip_rx_data *rdata )
{
    pjsip_dialog *dlg;
    pjmedia_sdp_session *local_sdp;
    pjsip_tx_data *tdata;
    unsigned options = 0;
    pj_status_t status;


    /* 
     * Respond (statelessly) any non-INVITE requests with 500 
     */
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) {

	if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {
	    pj_str_t reason = pj_str("Simple UA unable to handle "
				     "this request");

	    pjsip_endpt_respond_stateless( g_endpt, rdata, 
					   500, &reason,
					   NULL, NULL);
	}
	return PJ_TRUE;
    }


    /*
     * Reject INVITE if we already have an INVITE session in progress.
     */
    if (g_inv) {

	pj_str_t reason = pj_str("Another call is in progress");

	pjsip_endpt_respond_stateless( g_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
	return PJ_TRUE;

    }

    /* Verify that we can handle the request. */
    status = pjsip_inv_verify_request(rdata, &options, NULL, NULL,
				      g_endpt, NULL);
    if (status != PJ_SUCCESS) {

	pj_str_t reason = pj_str("Sorry Simple UA can not handle this INVITE");

	pjsip_endpt_respond_stateless( g_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
	return PJ_TRUE;
    } 

    /*
     * Create UAS dialog.
     */
    status = pjsip_dlg_create_uas( pjsip_ua_instance(), 
				   rdata,
				   NULL, /* contact */
				   &dlg);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_respond_stateless(g_endpt, rdata, 500, NULL,
				      NULL, NULL);
	return PJ_TRUE;
    }

    /* 
     * Get media capability from media endpoint: 
     */

    status = pjmedia_endpt_create_sdp( g_med_endpt, rdata->tp_info.pool, 1,
				       &g_med_skinfo, 
				       &local_sdp);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);


    /* 
     * Create invite session, and pass both the UAS dialog and the SDP
     * capability to the session.
     */
    status = pjsip_inv_create_uas( dlg, rdata, local_sdp, 0, &g_inv);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);


    /*
     * Initially send 180 response.
     *
     * The very first response to an INVITE must be created with
     * pjsip_inv_initial_answer(). Subsequent responses to the same
     * transaction MUST use pjsip_inv_answer().
     */
    status = pjsip_inv_initial_answer(g_inv, rdata, 
				      180, 
				      NULL, NULL, &tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);


    /* Send the 180 response. */  
    status = pjsip_inv_send_msg(g_inv, tdata); 
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);


    /*
     * Now create 200 response.
     */
    status = pjsip_inv_answer( g_inv, 
			       200, NULL,	/* st_code and st_text */
			       NULL,		/* SDP already specified */
			       &tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

    /*
     * Send the 200 response.
     */
    status = pjsip_inv_send_msg(g_inv, tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);


    /* Done. 
     * When the call is disconnected, it will be reported via the callback.
     */

    return PJ_TRUE;
}

 

/*
 * Callback when SDP negotiation has completed.
 * We are interested with this callback because we want to start media
 * as soon as SDP negotiation is completed.
 */
static void call_on_media_update( pjsip_inv_session *inv,
				  pj_status_t status)
{
    pjmedia_session_info sess_info;
    const pjmedia_sdp_session *local_sdp;
    const pjmedia_sdp_session *remote_sdp;
    pjmedia_port *media_port;

PJ_LOG(3,(THIS_FILE,"call_on_media_update"));

    if (status != PJ_SUCCESS) {

	app_perror(THIS_FILE, "SDP negotiation has failed", status);

	/* Here we should disconnect call if we're not in the middle 
	 * of initializing an UAS dialog and if this is not a re-INVITE.
	 */
	return;
    }

    /* Get local and remote SDP.
     * We need both SDPs to create a media session.
     */
    status = pjmedia_sdp_neg_get_active_local(inv->neg, &local_sdp);

    status = pjmedia_sdp_neg_get_active_remote(inv->neg, &remote_sdp);


    /* Create session info based on the two SDPs. 
     * We only support one stream per session for now.
     */
    status = pjmedia_session_info_from_sdp(inv->dlg->pool, g_med_endpt, 
					   1, &sess_info, 
					   local_sdp, remote_sdp);
    if (status != PJ_SUCCESS) {
	app_perror( THIS_FILE, "Unable to create media session", status);
	return;
    }

    /* If required, we can also change some settings in the session info,
     * (such as jitter buffer settings, codec settings, etc) before we
     * create the session.
     */

    /* Create new media session, passing the two SDPs, and also the
     * media socket that we created earlier.
     * The media session is active immediately.
     */
    status = pjmedia_session_create( g_med_endpt, &sess_info,
				     &g_med_transport, NULL, &g_med_session );
    if (status != PJ_SUCCESS) {
	app_perror( THIS_FILE, "Unable to create media session", status);
	return;
    }


    /* Get the media port interface of the first stream in the session. 
     * Media port interface is basicly a struct containing get_frame() and
     * put_frame() function. With this media port interface, we can attach
     * the port interface to conference bridge, or directly to a sound
     * player/recorder device.
     */
    pjmedia_session_get_port(g_med_session, 0, &media_port);
PJ_LOG(3,(THIS_FILE, "%d %d %d %d",
                media_port->info.clock_rate,    /* clock rate       */
            media_port->info.channel_count, /* channel count    */
            media_port->info.samples_per_frame, /* samples per frame*/
            media_port->info.bits_per_sample    /* bits per sample  */
        ));
#ifdef AUDIO



    /* Create a sound Player device and connect the media port to the
     * sound device.
     */
    status = pjmedia_snd_port_create_player( 
		    inv->pool,				/* pool		    */
		    -1,					/* sound dev id	    */
		    media_port->info.clock_rate,	/* clock rate	    */
		    media_port->info.channel_count,	/* channel count    */
		    media_port->info.samples_per_frame, /* samples per frame*/
		    media_port->info.bits_per_sample,   /* bits per sample  */
		    0,					/* options	    */
		    &g_snd_player);
    if (status != PJ_SUCCESS) {
	app_perror( THIS_FILE, "Unable to create sound player", status);
	PJ_LOG(3,(THIS_FILE, "%d %d %d %d",
	    	    media_port->info.clock_rate,	/* clock rate	    */
		    media_port->info.channel_count,	/* channel count    */
		    media_port->info.samples_per_frame, /* samples per frame*/
		    media_port->info.bits_per_sample    /* bits per sample  */
	    ));
	return;
    }

    status = pjmedia_snd_port_connect(g_snd_player, media_port);
#endif /* AUDIO */
#ifdef MICRO
    /* Create a sound recorder device and connect the media port to the
     * sound device.
     */
    status = pjmedia_snd_port_create_rec( 
		    inv->pool,				/* pool		    */
		    -1,					/* sound dev id	    */
		    media_port->info.clock_rate,	/* clock rate	    */
		    media_port->info.channel_count,	/* channel count    */
		    media_port->info.samples_per_frame, /* samples per frame*/
		    media_port->info.bits_per_sample,   /* bits per sample  */
		    0,					/* options	    */
		    &g_snd_rec);
    if (status != PJ_SUCCESS) {
	app_perror( THIS_FILE, "Unable to create sound recorder", status);
	return;
    }

    status = pjmedia_snd_port_connect(g_snd_rec, media_port);
#endif /* MICRO */
    /* Done with media. */

}


