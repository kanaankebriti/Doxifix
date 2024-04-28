#define RGBA 4
#define RGB 3

#include <gtk/gtk.h>

struct gui_info_struct {
	unsigned char tf_l;							// threshold filter lower limit
	unsigned char tf_u;							// threshold filter upper limit
	gboolean tf_stat;							// threshold filter enable/disable
	float exfv;								// exposure value
	gboolean ex_stat;							// threshold filter enable/disable
} gui_info = {0, 190, FALSE};							// set default values of gui input elements

GtkWidget *inpic;								// inserted picture widget
GdkPixbuf* inpic_obuf;								// original pixel data of inserted picture
GdkPixbuf* inpic_cbuf;								// a copy of inpic_obuf
gboolean mouse_pressed = FALSE;							// hold click press to pan

/* filters procedure */
void apply_filters () {
	GdkTexture *ipictxt;							// inserted picture texture
	guchar* inpic_cbuf_ptr;							// a pointer to the pixel data of the copy of inserted picture

	g_clear_object (&inpic_cbuf);						// clear previous pixel buffer
	inpic_cbuf = gdk_pixbuf_copy (inpic_obuf);				// INIT: make a copy of original pixbuf
	inpic_cbuf_ptr = gdk_pixbuf_get_pixels (inpic_cbuf);			// INIT: pixel data ptr for current picture
	unsigned int pixbuf_size = gdk_pixbuf_get_byte_length(inpic_obuf);// INIT: number of buffer pixels * bit depth
	int pic_type = gdk_pixbuf_get_n_channels (inpic_obuf);

	/* apply filters based on number of channels */
	switch (pic_type)
	{
	case RGBA:
	/* threshold filter */
	if (gui_info.tf_stat)							// apply threshold filter if asked for
		for (unsigned int i = 0; i < pixbuf_size; i += 4)
			if (inpic_cbuf_ptr[i] > gui_info.tf_u) {
				inpic_cbuf_ptr[i] = 255;
				inpic_cbuf_ptr[i + 1] = 255;
				inpic_cbuf_ptr[i + 2] = 255;
				inpic_cbuf_ptr[i + 3] = 255;
			}
			else if (inpic_cbuf_ptr[i] < gui_info.tf_l) {
				inpic_cbuf_ptr[i] = 0;
				inpic_cbuf_ptr[i + 1] = 0;
				inpic_cbuf_ptr[i + 2] = 0;
				inpic_cbuf_ptr[i + 3] = 255;
			}
	/* exposure */
	if (gui_info.ex_stat)							// apply exposure filter if asked for
		for (unsigned int i = 0; i < pixbuf_size; i += 4)
			if ((float)inpic_cbuf_ptr[i] * gui_info.exfv > 255) {
				inpic_cbuf_ptr[i] = 255;
				inpic_cbuf_ptr[i + 1] = 255;
				inpic_cbuf_ptr[i + 2] = 255;
				inpic_cbuf_ptr[i + 3] = 255;
			}
	break;
	case RGB:
	/* threshold filter */
	if (gui_info.tf_stat)							// apply threshold filter if asked for
		for (unsigned int i = 0; i < pixbuf_size; i += 3)
			if (inpic_cbuf_ptr[i] > gui_info.tf_u) {
				inpic_cbuf_ptr[i] = 255;
				inpic_cbuf_ptr[i + 1] = 255;
				inpic_cbuf_ptr[i + 2] = 255;
			}
			else if (inpic_cbuf_ptr[i] < gui_info.tf_l) {
				inpic_cbuf_ptr[i] = 0;
				inpic_cbuf_ptr[i + 1] = 0;
				inpic_cbuf_ptr[i + 2] = 0;
			}
	/* exposure */
	if (gui_info.ex_stat)							// apply exposure filter if asked for
		for (unsigned int i = 0; i < pixbuf_size; i += 3)
			if ((float)inpic_cbuf_ptr[i] * gui_info.exfv > 255) {
				inpic_cbuf_ptr[i] = 255;
				inpic_cbuf_ptr[i + 1] = 255;
				inpic_cbuf_ptr[i + 2] = 255;
			}
	break;
	}

	ipictxt = gdk_texture_new_for_pixbuf (inpic_cbuf);			// create texutre from the copy of pixel data
	gtk_picture_set_paintable (GTK_PICTURE(inpic), GDK_PAINTABLE(ipictxt));
	//gdk_texture_save_to_png (ipictxt, "cv.png");
	g_clear_object (&ipictxt);						// clear texture
}

/* CALLBACK: zoom on scroll */
void zoom_clb(	GtkEventControllerScroll* gesture,
		gdouble dx, gdouble dy,
		GtkWidget* inpic )
{
	static gfloat scale = 0.5f;
	if (!inpic_obuf) return;						// skip when no picture is available

	// determine scroll direction
	if (dy < 0)
		scale += .1f;							// scroll up
	else
		scale -= .1f;							// scroll down

	// minimum scale
	if (scale < .5f) {
		scale = .5f;
		return;
	}

	int w = gdk_pixbuf_get_width (inpic_obuf);
	int h = gdk_pixbuf_get_height (inpic_obuf);

	gtk_widget_set_size_request(inpic, w * scale, h * scale);
	gtk_widget_queue_draw(inpic);
}

/* CALBACK: on mouse motion */
void mm_clb (	GtkEventControllerMotion* self,
		gdouble x, gdouble y,
		GtkScrolledWindow *popan )
{
	static gdouble last_mouse_x, last_mouse_y;				// retain last location
	gdouble dx, dy;
	GtkAdjustment* popan_ha, * popan_va;

	// skip when no picture is available
	if (inpic_obuf)
		if (mouse_pressed) {
			popan_ha = gtk_scrolled_window_get_hadjustment(popan);
			popan_va = gtk_scrolled_window_get_vadjustment(popan);
			dx = last_mouse_x - x;
			dy = last_mouse_y - y;
			dx += gtk_adjustment_get_value(popan_ha);
			dy += gtk_adjustment_get_value(popan_va);
			// horizontal drag
			gtk_adjustment_set_value(popan_ha, dx);
			gtk_scrolled_window_set_hadjustment (popan, popan_ha);
			// vertical drag
			gtk_adjustment_set_value(popan_va, dy);
			gtk_scrolled_window_set_vadjustment (popan, popan_va);
			// redraw
			gtk_widget_queue_draw(inpic);
		} else {
			// store mouse (x,y) before click
			last_mouse_x = x;
			last_mouse_y = y;
		}
}

/* CALBACK: on mouse click press */
void mcp_clb (	GtkGestureClick* self,
		gint n_press,
		gdouble x, gdouble y,
		gpointer user_data )
{
	// skip when no picture is available
	if (inpic_obuf)
		mouse_pressed = TRUE;
}

/* CALBACK: on mouse click release */
void mcr_clb (	GtkGesture* gesture,
		GdkEventSequence* sequence,
		gpointer user_data )
{
	// skip when no picture is available
	if (inpic_obuf)
		mouse_pressed = FALSE;
}

/* CALLBACK: on open dialog close */
void open_cb (GObject *odiag, GAsyncResult *res, gpointer data) {
	GdkTexture *ipictxt;							// inserted picture texture
	GFile* ipicf;								// inserted picture file handler
	ipicf = gtk_file_dialog_open_finish (GTK_FILE_DIALOG(odiag), res, NULL);// open file
	// skip on cancel
	if (ipicf) {
		g_clear_object (&inpic_obuf);					// clear previous pixel buffer
		g_clear_object (&inpic_cbuf);					// clear previous pixel buffer
		inpic_obuf = gdk_pixbuf_new_from_file (	g_file_get_path(ipicf),
							NULL );			// retrive pixel data from picture file
		gdk_pixbuf_saturate_and_pixelate (	inpic_obuf,
							inpic_obuf,
							0.f,
							FALSE );		// convert to grayscale
		if (gui_info.tf_stat)						// perform same filters for new image
			apply_filters();
		else {
			ipictxt = gdk_texture_new_for_pixbuf (inpic_obuf);	// create texutre from pixel data
			gtk_picture_set_paintable (	GTK_PICTURE(inpic),
							GDK_PAINTABLE(ipictxt));// render texture
			g_clear_object (&ipictxt);				// clear texture
		}
	}
}

/* CALLBACK: click on insert button */
void insrt_sub (GtkWidget* widget, GtkWindow* main_window) {
	GtkFileDialog* odiag = gtk_file_dialog_new ();				// open picture dialog
	gtk_file_dialog_set_modal (odiag, TRUE);				// disable main window when choosing a new file

	/* filter based on file suffix */
	GListStore* suffix_list = g_list_store_new (GTK_TYPE_FILE_FILTER);
	GtkFileFilter* suffix_png = gtk_file_filter_new();
	GtkFileFilter* suffix_jpg = gtk_file_filter_new();
	gtk_file_filter_add_suffix(suffix_png, "png");
	gtk_file_filter_set_name(suffix_png, "PNG");
	g_list_store_append(suffix_list, suffix_png);
	gtk_file_filter_add_suffix(suffix_jpg, "jpg");
	gtk_file_filter_set_name(suffix_jpg, "JPG");
	g_list_store_append(suffix_list, suffix_jpg);
	gtk_file_dialog_set_filters(odiag, G_LIST_MODEL(suffix_list));

	gtk_file_dialog_open (odiag, main_window, NULL, &open_cb, NULL);	// open_cb will be called on open or cancel
}

/* CALLBACK: on save dialog close */
void save_cb (GObject *sdiag, GAsyncResult *res, gpointer data) {
	GFile* opic;
	opic = gtk_file_dialog_save_finish (GTK_FILE_DIALOG(sdiag), res, NULL);	// output picture
	// skip on cancel
	if (opic) {
		GString* opicp = g_string_new(g_file_get_path(opic));		// path to output picture
		g_string_append (opicp, ".png");				// append ".png" to the filename
		// skip when no picture is available
		if (inpic_obuf  != NULL)
			gdk_pixbuf_save (	inpic_cbuf,
						opicp->str,
						"png",
						NULL,
						"compression", "9", NULL );	// save to file
	}
}

/* CALLBACK: click on save button */
void svb_sub (GtkWidget* widget, GtkWindow* main_window) {
	GtkFileDialog* sdiag = gtk_file_dialog_new ();				// save picture dialog
	gtk_file_dialog_set_modal (sdiag, TRUE);				// disable main window when saving
	gtk_file_dialog_save (sdiag, main_window, NULL, &save_cb, NULL);	// save_cb will be called on save or cancel
}

/* CALLBACK: exposure state: active/deactive */
void exf_st (GtkWidget *this, GtkWidget *ex_slider) {
	// skip when no picture is available
	if (!inpic_obuf) {
		gtk_check_button_set_active (GTK_CHECK_BUTTON(this), FALSE);	// reset threshold filter availability
		return;
	}
	gboolean st = gtk_check_button_get_active (GTK_CHECK_BUTTON(this));	// get tf_check status
	gtk_widget_set_sensitive (ex_slider, st);				// set ex_slider state accordingly
	gui_info.ex_stat = st;							// make threshold filter available/inavailable accordingly
	apply_filters ();							// recalculate filters
}

/* CALLBACK: threshold filter state: active/deactive */
void tf_st (GtkWidget *this, GObject *tf_sliders) {
	// skip when no picture is available
	if (!inpic_obuf) {
		gtk_check_button_set_active (GTK_CHECK_BUTTON(this), FALSE);	// reset threshold filter availability
		return;
	}
	GtkWidget* tf_uval, *tf_lval;						// threshold filter upper and lower values
	tf_uval = g_object_get_data (tf_sliders, "tf_uval");			// retrive upper value
	tf_lval = g_object_get_data (tf_sliders, "tf_lval");			// retrive lower value
	gboolean st = gtk_check_button_get_active (GTK_CHECK_BUTTON(this));	// get tf_check status
	gtk_widget_set_sensitive (tf_uval, st);					// set tf_uval and tf_lval state accordingly
	gtk_widget_set_sensitive (tf_lval, st);
	gui_info.tf_stat = st;							// make threshold filter available/inavailable accordingly
	apply_filters ();							// recalculate filters
}

/* CALLBACK: on threshold filter upper limit value change */
void tfu_clb (GtkWidget *this, gpointer data) {
	gui_info.tf_u = gtk_range_get_value (GTK_RANGE(this));			// get upper limit slider value
	// adjust limits
	if (gui_info.tf_u <= gui_info.tf_l) {
		gui_info.tf_u = gui_info.tf_u + 1;
		gtk_range_set_value (GTK_RANGE(this), gui_info.tf_u);
		return;
	}
	apply_filters ();							// recalculate filters
}

/* CALLBACK: on exposure filter value change */
void exf_clb (GtkWidget *this, gpointer data) {
	gui_info.exfv = gtk_range_get_value (GTK_RANGE(this));			// get exposure filter value
	apply_filters ();							// recalculate filters
}

/* CALLBACK: on threshold filter upper limit value change */
void tfl_clb (GtkWidget *this, gpointer data) {
	gui_info.tf_l = gtk_range_get_value (GTK_RANGE(this));			// get lower limit slider value
	// adjust limits
	if (gui_info.tf_l > gui_info.tf_u) {
		gui_info.tf_l = gui_info.tf_u - 1;
		gtk_range_set_value (GTK_RANGE(this), gui_info.tf_l);
		return;
	}
	apply_filters ();							// recalculate filters
}

void activate (GtkApplication *app, gpointer user_data) {
	GtkWidget* mwin;							// main window
	GtkWidget* winpan;							// main pane container
	GtkWidget* cpan;							// control pane
	GtkWidget* popan;							// picture output pane

	GtkWidget* header;							// headerbar
	GtkWidget* hbtnsc;							// headerbar buttons container
	GtkWidget* open_btn_lbl, *open_btn_icn, *open_btn_box, *open_btn;	// insert picture button
	GtkWidget* save_btn_lbl, *save_btn_icn, *save_btn_box, *save_btn;	// save picture button

	GtkWidget *tf_box, *tf_check, *tf_uval, *tf_lval;			// threshold filter
	GtkWidget *exf_box, *exf_check, *exf_val;				// exposure filter

	/* main window */
	mwin = gtk_application_window_new (app);				// INIT: create main window
	gtk_window_set_title (GTK_WINDOW (mwin), "Doxifix");			// ATTR: set main window title

	/* headerbar*/
	header = gtk_header_bar_new ();						// INIT: new headerbar
	gtk_window_set_titlebar (GTK_WINDOW (mwin), header);			// attach headerbar to the main window
	hbtnsc = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);			// INIT: create headerbar buttons container
	gtk_box_set_homogeneous (GTK_BOX(hbtnsc), TRUE);			// ATTR: all headerbar buttons are of same width

	/* open button in headerbar */
	open_btn_icn = gtk_image_new_from_icon_name ("insert-image");		// INIT: icon for open button
	open_btn_lbl = gtk_label_new ("Insert");				// INIT: label for open button
	open_btn_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);		// INIT: container for icon + label
	open_btn = gtk_button_new ();						// INIT: open button itself
	gtk_box_set_homogeneous (GTK_BOX(open_btn_box), FALSE);			// ATTR: container decoration
	gtk_label_set_justify (GTK_LABEL(open_btn_lbl), GTK_JUSTIFY_CENTER);	// ATTR: label decoration
	gtk_box_append (GTK_BOX(open_btn_box), open_btn_icn);			// attach icon to the container
	gtk_box_append (GTK_BOX(open_btn_box), open_btn_lbl);			// attach label to the container
	gtk_button_set_child (GTK_BUTTON(open_btn), open_btn_box);		// attach container to open button
	g_signal_connect (open_btn, "clicked", G_CALLBACK (insrt_sub), mwin);	// SIG: click on insert button signal
	gtk_box_append (GTK_BOX(hbtnsc), open_btn);				// attach open_btn to buttons container

	/* save button in headerbar */
	save_btn_icn = gtk_image_new_from_icon_name ("document-save");		// INIT: icon for save button
	save_btn_lbl = gtk_label_new ("Save");					// INIT: label for save button
	save_btn_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);		// INIT: container for icon + label
	save_btn = gtk_button_new ();						// INIT: save button itself
	gtk_box_set_homogeneous (GTK_BOX(save_btn_box), FALSE);			// ATTR: container decoration
	gtk_label_set_justify (GTK_LABEL(save_btn_lbl), GTK_JUSTIFY_CENTER);	// ATTR: label decoration
	gtk_box_append (GTK_BOX(save_btn_box), save_btn_icn);			// attach icon to the container
	gtk_box_append (GTK_BOX(save_btn_box), save_btn_lbl);			// attach label to the container
	gtk_button_set_child (GTK_BUTTON(save_btn), save_btn_box);		// attach container to save button
	g_signal_connect (save_btn, "clicked", G_CALLBACK (svb_sub), mwin);	// SIG: click on save button signal
	gtk_box_append (GTK_BOX(hbtnsc), save_btn);

	gtk_header_bar_pack_start (GTK_HEADER_BAR (header), hbtnsc);		// attach buttons container to the header bar

	/* main window layout */
	winpan = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);			// INIT: create paned view
	cpan = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);			// INIT: create control pane
	popan = gtk_scrolled_window_new ();					// INIT: create picture output pane
	inpic = gtk_picture_new ();						// INIT: create picture object

	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW(popan), inpic);	// attach picture to pane
	gtk_paned_set_start_child (GTK_PANED (winpan), cpan);			// attach control pane
	gtk_paned_set_end_child (GTK_PANED (winpan), popan);			// attach picture output pane

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(popan),
					GTK_POLICY_ALWAYS,
					GTK_POLICY_ALWAYS);			// picture output window decorations

	gtk_widget_set_size_request (cpan, 300, 600);				// control pane intial size
	gtk_widget_set_size_request (popan, 500, 600);				// picture output intial size
	gtk_paned_set_resize_start_child (GTK_PANED (winpan), FALSE);		// main pane decorations
	gtk_paned_set_shrink_start_child (GTK_PANED (winpan), FALSE);
	gtk_paned_set_resize_end_child (GTK_PANED (winpan), TRUE);
	gtk_paned_set_shrink_end_child (GTK_PANED (winpan), FALSE);

	gtk_window_set_child (GTK_WINDOW (mwin), winpan);			// attach paned view to the main window

	/* control pane */
	// threshold filter
	tf_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);			// INIT: threshold filter item
	tf_check = gtk_check_button_new_with_label ("threshold filter");	// INIT: threshold filter checkbox
	tf_uval = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL
						, 0, 255, 1);			// INIT: threshold filter upper value
	tf_lval = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL
						, 0, 255, 1);			// INIT: threshold filter upper value
	gtk_scale_set_draw_value (GTK_SCALE(tf_uval), TRUE);			// ATTR: upper value slider decorations
	gtk_scale_set_draw_value (GTK_SCALE(tf_lval), TRUE);			// ATTR: lower value slider decorations
	gtk_widget_set_sensitive (tf_uval, FALSE);				// ATTR: deactivate upper value slider at first till tf_check
	gtk_widget_set_sensitive (tf_lval, FALSE);				// ATTR: deactivate lower value slider at first till tf_check
	gtk_range_set_value (GTK_RANGE(tf_uval), gui_info.tf_u);		// ATTR: threshold filter upper limit default value
	gtk_range_set_value (GTK_RANGE(tf_lval), gui_info.tf_l);		// ATTR: threshold filter upper lower default value

	gtk_box_append (GTK_BOX(tf_box), tf_check);				// attach checkbox to threshold filter container
	gtk_box_append (GTK_BOX(tf_box), tf_uval);				// attach upper value slider to threshold filter container
	gtk_box_append (GTK_BOX(tf_box), tf_lval);				// attach lower value slider to threshold filter container
	gtk_box_append (GTK_BOX(cpan), tf_box);					// attach threshold filter item to control pane

	g_signal_connect (tf_uval, "value-changed", G_CALLBACK (tfu_clb), NULL);// SIG: attach threshold filter signal on upper limit value change
	g_signal_connect (tf_lval, "value-changed", G_CALLBACK (tfl_clb), NULL);// SIG: attach threshold filter signal on lower limit value change
	g_object_set_data (G_OBJECT(tf_check), "tf_uval", tf_uval);		// SIGHACK: append tf_uval to tf_check and name it for later recovery
	g_object_set_data (G_OBJECT(tf_check), "tf_lval", tf_lval);		// and preventing globals
	g_signal_connect (tf_check, "toggled", G_CALLBACK (tf_st), tf_check);	// SIG: attach threshold filter signal on activation/deactivation

	// exposure filter
	exf_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);			// INIT: exposure filter item
	exf_check = gtk_check_button_new_with_label ("exposure");		// INIT: exposure filter checkbox
	exf_val = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL
						, 1.f, 3.f, .1f);		// INIT: exposure filter value

	gtk_scale_set_draw_value (GTK_SCALE(exf_val), TRUE);			// ATTR: exposure filter value slider decorations
	gtk_widget_set_sensitive (exf_val, FALSE);				// ATTR: deactivate exposure filter value slider at first till ex_check

	gtk_box_append (GTK_BOX(exf_box), exf_check);				// attach checkbox to exposure filter container
	gtk_box_append (GTK_BOX(exf_box), exf_val);				// attach upper value slider to exposure filter container
	gtk_box_append (GTK_BOX(cpan), exf_box);					// attach exposure filter item to control pane

	g_signal_connect (exf_val, "value-changed", G_CALLBACK (exf_clb), NULL);// SIG: attach exposure filter signal on upper limit value change
	g_signal_connect (exf_check, "toggled", G_CALLBACK (exf_st), exf_val);	// SIG: attach exposure filter signal on activation/deactivation

	/* render */
	gtk_window_present (GTK_WINDOW (mwin));

	/* events */
	// mouse scroll
	GtkEventController* scroll_event =
		gtk_event_controller_scroll_new (
			GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
			GTK_EVENT_CONTROLLER_SCROLL_DISCRETE
		);
	gtk_widget_add_controller(inpic, scroll_event);
	g_signal_connect(scroll_event, "scroll", G_CALLBACK(zoom_clb), inpic);
	// mouse click
	GtkGesture* click_event = gtk_gesture_click_new();
	gtk_gesture_single_set_button (
		GTK_GESTURE_SINGLE(click_event),
		GDK_BUTTON_PRIMARY
	);
	gtk_widget_add_controller(inpic, GTK_EVENT_CONTROLLER(click_event));
	g_signal_connect(click_event, "pressed", G_CALLBACK(mcp_clb), inpic);
	g_signal_connect(click_event, "released", G_CALLBACK(mcr_clb), inpic);
	// mouse motion
	GtkEventController* mm_event = gtk_event_controller_motion_new();
	gtk_widget_add_controller(inpic, mm_event);
	g_signal_connect(mm_event, "motion", G_CALLBACK(mm_clb), popan);
}

int main (int argc, char **argv) {
  GtkApplication *app;
  int status;

  app = gtk_application_new (	"com.github.kanaankebriti.doxifix",
  				G_APPLICATION_DEFAULT_FLAGS );
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
