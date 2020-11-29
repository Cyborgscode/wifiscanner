
/* LICENSE 

    Author: Marius Schwarz - support@evolution-hosting.eu  Braunschweig, Germany
    Date of Release: 8.2.2019
    Revision: 0.99

    This SOFTWARE is provided as is. No warrenties of any sort, that it won't damage something. 

    The use is free of charge of any private person and commercial businesses,
    but you are limited to : 

	- name the original author
	- mark changes you made to the source as yours
        - make it publically available again, free of charge ofcourse and under the same license.

*/


#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

int global_surpressor = 1;

char *channels;
char *cells;

char **c24names;
char **c5names;

GtkWidget *redraw_this;
cairo_t	 *redraw_cr;
int modus = 24; // 50 // 24  ( Default Scanmode ) 
int scaninprogress = 0;
GObject *infobox;
GtkTextBuffer *infobuffer;
double dpi;

/// The real timer init, there was a reason to have it here at the beginning... 

timer_t gTimerid;

void start_timer(void)
{
struct itimerspec value;

value.it_value.tv_sec = 10;//waits for 10 seconds before sending timer signal
value.it_value.tv_nsec = 0;

value.it_interval.tv_sec = 10;//sends timer signal every 10 seconds
value.it_interval.tv_nsec = 0;

timer_create (CLOCK_REALTIME, NULL, &gTimerid);

timer_settime (gTimerid, 0, &value, NULL);

}

/* used for GTK + APP ICON -- atm unsused */
	
GdkPixbuf *create_pixbuf(const gchar * filename) {
		
	 GdkPixbuf *pixbuf;
	 GError *error = NULL;
	 pixbuf = gdk_pixbuf_new_from_file(filename, &error);
	 
	 if (!pixbuf) {
			 
			fprintf(stderr, "%s\n", error->message);
			g_error_free(error);
	 }

	 return pixbuf;
}

/* read in a shell command and copy it to the buffer
 
   maxlen sets the maximum number of bytes to be readinto the buffer to make bufferoverflows impossible

 */

char *readPipe(char *cmd,char *buffer, int maxlen) {

	memset(buffer, 0, maxlen); // clear buffer before using it, incase the new result, is smaller than the old one => crashing the app

	FILE *fp;
	fp = popen(cmd, "r");
	if (fp == NULL) {
		printf("Failed to run %s\n", cmd );
		return FALSE;
	}
	char path[2049];
	char *cp = buffer;
	int len = 0, readin = 0;
	
	while (fgets(path, sizeof(path)-1, fp) != NULL) {
		if ( path != NULL ) {
			len = strlen(path);
			readin = readin +len;
			if ( len > 0 && readin < maxlen ) {
				strncpy(cp, path, len );
				cp+= len;
			}
		}
	}
	pclose(fp);
	// printf("Ende ReadPipe(%s)\n result=%s\n\n",cmd,buffer);
}

/* Splits a given String into chunks, marked by delimiter[0] and gives them back as an array */

char **split(char *buffer, char *delimiter) {
	

	// Copy the pointer 
	char *idx = buffer;

	// account for occurance of delimiter
	int count = 0;
	for(int i=0;i<strlen(buffer);i++) 
		if ( buffer[i] == delimiter[0] ) 
			count++;

	// if not found , exit with error
	if ( count < 1 ) {
		return NULL;
	}

//	printf( "Zählung durch : %d\n", count); 

	// make space for an array of strings + 1 ( NULL ) 
	char **result = malloc( sizeof(&buffer)*(count+1) );
	long element = 0;

	// actual Splitfunction : 
	char *ptr = strtok( buffer, delimiter );
	while ( ptr != NULL ) {
		char *x = malloc( strlen(ptr) + 2);
		strncpy( x, ptr, strlen(ptr)+1 );
		result[element++] = x;
		ptr = strtok( NULL, "\n" );
	}
	result[element] = NULL; // Letztes Element NULL Terminieren.
	return result;

}

// simple counterfunction for array elements

int count(char **array) {
	int found = 0;
	if ( array == NULL ) return 0;
	for(int i=0; array[i]!=NULL ;i++) found++;
	return found;
}

// create array with the platform size of bytes for (n+1)*size(ptr)

char **newArray(char **liste, int anz) {
	char **array = malloc( sizeof(&liste)* ( anz+2 ) );
	for(int i=0;i<anz+1;i++) array[i] = NULL;	// Init it with zeros
	return array;
}

// copy a String, but as a hardcopy, so we have pointers to real memory in  the resulting array of Strings

char *cloneString(char *s) {
	char *n = malloc( strlen(s)+2 );
	if ( n ) {
		strncpy( n,s, strlen(s)+1 );
		return n;
	}
	return NULL;
}

// NO RESIZE atm .. would be nice.

static void reactToResize(GtkWidget *widget,gpointer	 data) {
	// g_print ("Window Event\n");
}

// write message to textbuffer and force GTK to update that textbuffer instantly

void updateMessage(char *msg) {
	gtk_text_buffer_set_text(infobuffer, msg ,-1);
	gtk_widget_queue_draw ( GTK_WIDGET( infobox ));

	// FORCE GTK TO Update something! 
	while (gtk_events_pending())
		gtk_main_iteration();

}

/* OK, your reached the most important part .. time to use base64 and AES256 to protect the property -- just kidding */

gboolean redraw(GtkWidget *widget, cairo_t *cr, gpointer data) {


	// As this function is called 2 times, while initializing the programm, we ignore the first one to speed things up 
	if ( global_surpressor > 0 ) {
		global_surpressor--;
		return FALSE;
	}

	// Set a Message for the user, that knows why his buttons are jammed atm

	updateMessage("Scanning ...");		

	// Simple protection against timer based interference
	// two scans can't happen at the same time, as the hw is busy with one.

	scaninprogress = 1;

	// the actual scan is outsourced to iwlist

	readPipe("/usr/sbin/iwlist scanning 2>/dev/null", cells, 500000);
		
//	printf("debug: %s\n",cells);
		
	// if we get a valid busy message OR the cells buffer only contains a single line, we quit.

	if ( strstr( cells , "Device or resource busy") != NULL || strlen(cells) < 100 ) {
		scaninprogress = 0;
		updateMessage("Device is busy ...");
		return FALSE;
	}

	updateMessage("paiting ...");

	// init some vars..

	guint width, height;
	GdkRGBA color;
	GtkStyleContext *context;

	// make copy of important vars to have them inside rescan...() functions
	
	redraw_this = widget;
	redraw_cr   = cr;

	// get some basic infos 
	
	context = gtk_widget_get_style_context (widget);

	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);

	gtk_render_background (context, cr, 0, 0, width, height);
/*
	gtk_style_context_get_color (context,
		 gtk_style_context_get_state (context),
		 &color);
*/

	// Set a default color to paint 

	color.red = 0x8000;
	color.green = 0x8000;
	color.blue = 0x8000;
	color.alpha = 0x8000;
	gdk_cairo_set_source_rgba (cr, &color);

	// here you can change the appearance ..

	double bsize = 10.0;	// len of horizontal unit markers
	double abstand = 10.0;  // general distance for the left corner of the drawarea for the vertical axis
	double fontsize = 16;    // 
	double hnull = height-abstand-bsize-fontsize-2;	// vertical position of the x-Axis 
	int dh = hnull-abstand;
	int dw = width-2*abstand;

//	printf("dw = %f w=%d\n", dw, width);

	// how many colors do have defined for the wifi signal curves , the more the better! 

	const int anzf = 13;

	double farben[13*3] = { 1.0,1.0,1.0, 1.0,0.0,0.0, 0.0,1.0,0.0, 0.0,0.0,1.0, 1.0,0.7,0.7, 0.7,1.0,0.7,	0.7,0.7,1.0, 0.5,0.5,0.5, 1.0,0.8,0.0,
				1.0,1.0,0.0, 0.55,0.3,0.05, 1.0,0.0,1.0, 0.54,0.05,0.42 };

	// Set font for drawing

	cairo_set_font_size(cr, fontsize);

	// draw axis 

	cairo_move_to (cr, width, hnull );
	cairo_line_to (cr, bsize + abstand, hnull );
	cairo_line_to (cr, bsize + abstand, abstand );
	cairo_stroke(cr);

	// draw unit labels horizontal

	for(int i=0;i<=dh;i+=(dh/10)) {
		color.red = 0x8000;
		color.green = 0x8000;
		color.blue = 0x8000;
		color.alpha = 0x8000;
		gdk_cairo_set_source_rgba (cr, &color);

		cairo_move_to (cr, abstand, abstand+i );
		cairo_line_to (cr, bsize + abstand, abstand+i );
		cairo_stroke(cr);

		if ( i != dh ) {
			// draw a not so hard unit axis into the curve background
			cairo_set_source_rgb (cr, 0.3, 0.3, 0.3	);
			cairo_move_to (cr, bsize + abstand+1, abstand+i );
			cairo_line_to (cr, bsize + abstand+dw, abstand+i );
			cairo_stroke(cr);
		}
	}

	// reset main color.
	color.red = 0x8000;
	color.green = 0x8000;
	color.blue = 0x8000;
	color.alpha = 0x8000;
	gdk_cairo_set_source_rgba (cr, &color);


	int anz = 0;

	// check how many channels we have for the used scanning mode

	if ( modus == 24 ) anz = count( c24names );
	if ( modus == 50 ) anz = count( c5names );

	// draw vertical channel markings and names : 

	int j=0;
	for(int i=(dw/anz);i<=dw;i+=(dw/anz)) {
		
		char buffer[10];

		cairo_move_to (cr, abstand+ bsize+ i, hnull );
		cairo_line_to (cr, abstand+ bsize+ i, hnull+bsize );
		cairo_stroke(cr);
		cairo_move_to (cr, abstand+ bsize+ i-(fontsize/2), hnull+bsize+2+fontsize );
//		printf("index=%d\n", j);
		if ( modus == 24 ) sprintf(buffer, "%s", c24names[j++]);
		if ( modus == 50 ) sprintf(buffer, "%s", c5names[j++]);
		cairo_show_text(cr, buffer);
	}

	// axis model is ready, now the content : \o/ 

	char **lines = split(cells,"\n");
	char *channel;
	char *essid;
	char *level = malloc(100);
	
	// we parse the content of iwlist scanning directly : 

	int farbe = 0;
	for(int i=0; lines[i]!=NULL ;i++) {
		if ( strstr( lines[i], "(Channel " ) != NULL ) {
//			printf("DEBUG: Channel => %s\n", lines[i]);
			if ( strstr( lines[i], ")") > 0 ) {
				channel = malloc(20);
				memset( channel, 0, 20 );
				char *position = strstr( lines[i], "Channel " ) + strlen("Channel ");
				if ( strlen(position) < 5 ) {
					char *endpos = strstr( position, ")");
					if ( endpos > position && ( endpos-position) < 5 ) {
						strncpy(channel, position, endpos-position);
					} else channel = position; // EMERG measure later strcmp will fail, but no exploit possible
				} else channel = position; // EMERG measure later strcmp will fail, but no exploit possible
			
			} else {
				channel = strstr( lines[i], "Channel " ) + strlen("Channel ");
			}
//			printf("DEBUG: Channel => |%s|\n", channel);
			
		}
		if ( strstr( lines[i], "ESSID:" ) != NULL )		 essid = strstr( lines[i], "ESSID:" ) + strlen("ESSID:") ;
		if ( strstr( lines[i], "level=" ) != NULL ) {
			char *von = strstr( lines[i], "level=" ) + strlen("level=");
			if ( strlen( von ) < 100 ) {
				memset( level, 0, strlen(von)+1 );
				strncpy( level, von, strlen(von));
			} else  printf("Debug: Scan: level feld zu lang => %s\n", von);
		}
		if ( strstr( lines[i], "Extra:fm" ) != NULL ) {

			// if we hit mode: we have all we need for now

			char **z;
			if ( modus == 24 ) z = c24names;
			if ( modus == 50 ) z = c5names;

			for(int k=0; z[k] != NULL; k++) {
				// check, what index is the found channel on the axis .. AS channels are NOT read in a straight order.. no idea what iwlist thinks it does :(
				if ( strcmp( z[k], channel) == 0 ) {

//					printf("Debug: Scan k=%0d dw=%0d dh=%0d anz=%0d abstand=%0f bsize=%0f level=%s\n",k,dw,dh,anz,abstand,bsize,level);


					// Calculate start, mid and endpoint of the curve : 

					int m =	(k+1) * (dw/anz)+abstand+bsize;
					char **args = split(level,"/");
//					int h = dh * (( 0 - atoi( args[0] ) ) +30)  / 90 ; // +30 DB because signals are weak. We strech the results displayed that way a bit.

					int h = 2 * dh * atoi( args[0] )/90; // FEDORA 33+AARCH64+ Iwtools version 29r26

					
					int m1=	m-(dw/anz);
					int m2=	m+(dw/anz);
					if ( modus == 24 ) {
						// 2.4 Ghz Wifi uses 20 Mhz bandwidth => 4(+/-2) channels jammed 
						// 2.4 Ghz 1 Channel = 5 Mhz
					
						m1=m-(2*dw/anz);
						m2=m+(2*dw/anz);
					}
					if ( m1 < (abstand+bsize) )	m1 = abstand+bsize;
					
					// printf(" channel=%s  name=%s dh=%0d h=%0d level=%s m=%0d m1=%0d m2=%0d\n", channel, essid, dh, h, level, m,m1,m2);

					// Set the color for this curve. We rotate the colors with ( count mod anz ). +x is needed due to rgb storage

					cairo_set_source_rgb (cr, farben[ (farbe%anzf)*3+0 ], farben[(farbe%anzf)*3+1],farben[ (farbe%anzf)*3+2 ]	);

					// Paint curve and the name of the Wifinetworks ESSID 

					cairo_stroke(cr);
					cairo_curve_to(cr, m1,hnull , m,hnull-h, m2 ,hnull );
					cairo_stroke(cr);
					cairo_move_to(cr, m-strlen(essid)*(fontsize/2),hnull-(h*35/100) );
					cairo_show_text(cr, essid );
					cairo_stroke(cr);
					farbe++;
				}
			}
		}
	}
	scaninprogress = 0;
	updateMessage("Idle ...");
//	printf("Debug: Ende Scan\n");

	return FALSE;
}

// next scan will be 2.4ghz for sure, if we have 2.4ghz ( you never know )

static void switch_24 (GtkWidget *widget, gpointer	 data) {
	if ( count(c24names) > 0 ) {
		modus = 24;
		updateMessage("Switching to 2.4Ghz band");
	} else  updateMessage("2.4Ghz band not available");	
//	printf("Switch 24\n");
}

// next scan will be 5ghz, if we have 5ghz ( you never know )

static void switch_5 (GtkWidget *widget, gpointer	 data) {
	if ( count(c5names) > 0 ) {
		modus = 50;
		updateMessage("Switching to 5Ghz band");
	} else  updateMessage("5Ghz band not available");

}

// do a scan, IF NONE IS RUNNING ATM

static void scan_24 (GtkWidget *widget, gpointer data) {
	if ( scaninprogress == 0 ) {
		scaninprogress = 1;
		redraw( redraw_this,redraw_cr, data);
	}
	gtk_widget_queue_draw ( redraw_this );
	
}

// do a scan, IF NONE IS RUNNING ATM

// yes, double content .. two buttons with the same callback .. if that would work with GTK without errors ?

static void scan_5 (GtkWidget *widget, gpointer	data) {

	if ( scaninprogress == 0 ) {
		scaninprogress = 1;
		redraw( redraw_this,redraw_cr, data);
	}
	gtk_widget_queue_draw ( redraw_this );
}

// do a screenshot via gnome-screenshot
// if you wanne have it in the user directory, you need to figer out which user started the -gtk wrapper script
// because WE run as root .. in case you missed the screenshots

static void screenshot (GtkWidget *widget, gpointer	 data) {

	char *dummy = malloc(10000);
	readPipe("gnome-screenshot;gnome-screenshot -c", dummy, 10000);

}

// We want actual scans .. every 10 seconds..

void timer_callback(int sig) {

//	printf(" Catched timer signal: %d ... !!\n", sig);
	if ( redraw_this == NULL ) return; // if the drawing routine has not run yet, we can't init a scan, as we have  no idea whats the widgets p* is.
	if ( modus == 24 ) scan_24(NULL,NULL);
	if ( modus == 50 ) scan_5(NULL,NULL);
}

// If you ever wanted to start( hating )GTK.. start here.. it helps a lot :D

int main (int	 argc, char *argv[]) {

	GtkBuilder *builder;
	GObject *drawing_area;
	GObject *window; 
	GObject *button;
	GdkPixbuf *icon;
	GError *error = NULL;

	gtk_init(&argc, &argv);

	// get us some ram 

	channels = malloc(50000);
	cells	 = malloc(500000);

	// we only need one channel list of this hw :

	readPipe("/usr/sbin/iwlist channel 2>/dev/null", channels,50000);

	char **liste = split(channels,"\n");
	if ( liste == NULL ) {
		printf("Keine Channels gefunden!\n");
		return FALSE;
	}

	int c24 = 0;
	int c5	= 0;

	// Count how many elements we need in the namearrays
	
	for(int i=0; liste[i]!=NULL ;i++) {
		if ( strstr(liste[i],": 2.4") != NULL ) c24++;
		if ( strstr(liste[i],": 5.") != NULL ) c5++;
	}

	// create namearrays

	c24names = newArray(liste, c24);
	 c5names = newArray(liste, c5);
	
	// copy names 

	c5 = 0; c24 = 0;

	for(int i=0; liste[i]!=NULL ;i++) {
		char *p = strstr( liste[i], " Channel ");
		if ( p ) {
			char **args = split( p + strlen(" Channel ") , " : " );
			// printf("found channel : %s %s\n", args[0], args[1] );
			if ( strstr(args[1],"2.4") != NULL ) {
				if (strstr( args[0], "0") == args[0] ) {
					c24names[c24++] = cloneString( args[0]+1 );
				} else	c24names[c24++] = cloneString( args[0] );
			} 
			if ( strstr(args[1],"5.")	!= NULL ) c5names[c5++] = cloneString( args[0] );
		}
	}

	// DEBUG namearrays

//	for(int i=0; c24names[i]!=NULL ;i++) printf("%d = %s\n", i,c24names[i] );
//	for(int i=0; c5names[i]!=NULL ;i++) printf("%d = %s\n", i,c5names[i] );

	// IF (a big if as you can see) we ever have NO 2.4ghz channels, switch to 5ghz as the default scanmode :
	
	if ( count(c24names) < 1 && count(c5names) > 0 ) {
		modus = 50;
	}

	/* START GUI WORK */

	/* Construct a GtkBuilder instance and load our UI description */
	builder = gtk_builder_new ();
	if (gtk_builder_add_from_file (builder, "/usr/share/wifiscanner.ui", &error) == 0)
		{
			g_printerr ("Error loading file: %s\n", error->message);
			g_clear_error (&error);
			return 1;
		}

	/* Connect signal handlers to the constructed widgets. */
	window = gtk_builder_get_object (builder, "window");
	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	/* GET SCREEN SIZE 

	 GdkScreen * screen = gtk_window_get_screen(GTK_WINDOW(window));
	 *widthret = gdk_screen_get_width(screen); // in pixels
	 *heightret = gdk_screen_get_height(screen);

	ist bestimmt mal nützlich ... 

	*/

	dpi = gdk_screen_get_resolution ( gtk_window_get_screen(GTK_WINDOW(window)) );

	/* Set black background */

	GtkCssProvider* provider = gtk_css_provider_new();
	GdkDisplay* display = gdk_display_get_default();
	GdkScreen* screen = gdk_display_get_default_screen(display);
	gtk_style_context_add_provider_for_screen(screen,
		GTK_STYLE_PROVIDER(provider),
		GTK_STYLE_PROVIDER_PRIORITY_USER);
	gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider),
		"#wifiscanner,window { background-color: black; }\n#wifiscanner,GtkTextView#infobox { background-color: black; color: white; padding: 5px; } ",
		-1, NULL);
	g_object_unref(provider);
	
	// connect buttons and other elements with theire callback functions: 

	button = gtk_builder_get_object (builder, "button1");
	g_signal_connect (button, "clicked", G_CALLBACK ( switch_24 ), NULL);

	button = gtk_builder_get_object (builder, "button2");
	g_signal_connect (button, "clicked", G_CALLBACK ( switch_5 ), NULL);

	button = gtk_builder_get_object (builder, "place3");
	g_signal_connect (button, "clicked", G_CALLBACK ( screenshot ), NULL);

	button = gtk_builder_get_object (builder, "quit");
	g_signal_connect (button, "clicked", G_CALLBACK (gtk_main_quit), NULL);

	drawing_area = gtk_builder_get_object (builder, "imagecontainer");
	g_signal_connect (drawing_area, "draw", G_CALLBACK (redraw), NULL);

	// a bit of magic to get a textview as a messageboard.

	infobox = gtk_builder_get_object (builder, "infobox");
	infobuffer = gtk_text_view_get_buffer ( GTK_TEXT_VIEW( infobox ) );
	gtk_text_view_set_left_margin (GTK_TEXT_VIEW ( infobox ), 10);
	gtk_text_view_set_top_margin (GTK_TEXT_VIEW ( infobox ), 10);

	// some day in the future this will be helpful
	updateMessage("waiting for device...");

	g_signal_connect(G_OBJECT(window), "configure-event", G_CALLBACK(reactToResize), NULL); 

	// start the time for a periodically scan : 

	(void) signal(SIGALRM, timer_callback);
			start_timer();

	// that it .. tada here it comes ... 
	gtk_main ();

	return 0;
}


