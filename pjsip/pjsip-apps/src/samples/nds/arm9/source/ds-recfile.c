/* $Id$ */
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
 * \page page_pjmedia_samples_recfile_c Samples: Capturing Audio to WAV File
 *
 * In this example, we capture audio from the sound device and save it to
 * WAVE file.
 *
 * @see page_pjmedia_samples_playfile_c
 *
 * This file is pjsip-apps/src/samples/recfile.c
 *
 * \includelineno recfile.c
 */

#include <pjmedia.h>
#include <pjlib.h>

#include <stdio.h>

#include <nds.h>
#include <fat.h>
#include <dswifi9.h>
#include "arm9_wifi.h"

/* For logging purpose. */
#define THIS_FILE   "recfile.c"


/* Configs */
//#define CLOCK_RATE	    44100
//#define CLOCK_RATE      16000
#define CLOCK_RATE 8000
//#define SAMPLES_PER_FRAME   (CLOCK_RATE * 20 / 1000)
#define SAMPLES_PER_FRAME 160
//#define NCHANNELS	    2
#define NCHANNELS       1
#define BITS_PER_SAMPLE	    16


/*static const char *desc = 
 " FILE					    \n"
 "  recfile.c				    \n"
 "					    \n"
 " PURPOSE:				    \n"
 "  Record microphone to WAVE file.	    \n"
 "					    \n"
 " USAGE:				    \n"
 "  recfile FILE.WAV			    \n"
 "";*/


/* Util to display the error message for the specified error code  */
static int app_perror( const char *sender, const char *title, 
		       pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    PJ_UNUSED_ARG(sender);

    pj_strerror(status, errmsg, sizeof(errmsg));

    printf("%s: %s [code=%d]\n", title, errmsg, status);
    return 1;
}

static pj_oshandle_t         file; /* Log file */

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
 */
int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    pj_pool_t *pool;
    pjmedia_port *file_port;
    pjmedia_snd_port *snd_port;
    pj_status_t status;
	char *filename="record.wav";

    /* Verify cmd line arguments. */
    /*if (argc != 2) {
	puts("");
	puts(desc);
	return 0;
    }*/
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

    pj_file_open( NULL, "logsimple.txt", PJ_O_WRONLY, &file);
    pj_log_set_log_func(&log_write_file);

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

    /* 
     * Initialize media endpoint.
     * This will implicitly initialize PJMEDIA too.
     */
#if PJ_HAS_THREADS
    status = pjmedia_endpt_create(&cp.factory, NULL, 1, &med_endpt);
#else
	status = pjmedia_endpt_create(&cp.factory, NULL, 0, &med_endpt);
#endif
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Create memory pool for our file player */
    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "app",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
			   );

    /* Create WAVE file writer port. */
    status = pjmedia_wav_writer_port_create(  pool, filename,
					      CLOCK_RATE,
					      NCHANNELS,
					      SAMPLES_PER_FRAME,
					      BITS_PER_SAMPLE,
					      0, 0, 
					      &file_port);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to open WAV file for writing", status);
	return 1;
    }

    /* Create sound player port. */
    status = pjmedia_snd_port_create_rec( 
		 pool,				    /* pool		    */
		 -1,				    /* use default dev.	    */
		 file_port->info.clock_rate,	    /* clock rate.	    */
		 file_port->info.channel_count,	    /* # of channels.	    */
		 file_port->info.samples_per_frame, /* samples per frame.   */
		 file_port->info.bits_per_sample,   /* bits per sample.	    */
		 0,				    /* options		    */
		 &snd_port			    /* returned port	    */
		 );
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to open sound device", status);
	return 1;
    }

    /* Connect file port to the sound player.
     * Stream playing will commence immediately.
     */
    status = pjmedia_snd_port_connect( snd_port, file_port);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);



    /* 
     * Recording should be started now. 
     */


    /* Sleep to allow log messages to flush */
    pj_thread_sleep(10);


    printf("Recording %s..\n", filename);
    printf("Press <A> to stop recording and quit\n");

    //fgets(tmp, sizeof(tmp), stdin);
	while(1) 
    {
        swiWaitForVBlank();
        scanKeys();
        uint32 key = keysDown(); 
        if (key & KEY_A)
        {
            break;
        }
    }       
    printf("bye bye\n");

    
    /* Start deinitialization: */

    /* Destroy sound device */
    status = pjmedia_snd_port_destroy( snd_port );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Destroy file port */
    status = pjmedia_port_destroy( file_port );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Release application pool */
    pj_pool_release( pool );

    /* Destroy media endpoint. */
    pjmedia_endpt_destroy( med_endpt );

    /* Destroy pool factory */
    pj_caching_pool_destroy( &cp );

	pj_file_close(file);
	
    /* Shutdown PJLIB */
    pj_shutdown();


    /* Done. */
    return 0;
}
