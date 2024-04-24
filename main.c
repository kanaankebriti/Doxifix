#include <gtk/gtk.h>

struct gui_info_struct {
	unsigned char tf_l;							// threshold filter lower limit
	unsigned char tf_u;							// threshold filter upper limit
	gboolean tf_stat;							// threshold filter enable/disable
} gui_info = {0, 190, FALSE};							// set default values of gui input elements

GtkWidget *in_pic;								// inserted picture widget
GdkPixbuf* inpic_obuf;								// original pixel data of inserted picture
GdkPixbuf* inpic_cbuf;								// a copy of inpic_obuf
gboolean mouse_pressed = FALSE;							// hold click press to pan

/* filters procedure */
void apply_filters () {
	GdkTexture *ipictxt;							// inserted picture texture
	const unsigned int pixbuf_size = gdk_pixbuf_get_byte_length(inpic_obuf);
	g_clear_object (&inpic_cbuf);						// clear previous pixel buffer
	inpic_cbuf = gdk_pixbuf_copy (inpic_obuf);				// make a copy of original pixbuf
	guchar* inpic_cbuf_ptr;							// a pointer to the pixel data of the copy of inserted picture
	inpic_cbuf_ptr = gdk_pixbuf_get_pixels (inpic_cbuf);			// pixel data ptr for current picture

	/* threshold filter */
	if (gui_info.tf_stat)
		for (unsigned int i = 0; i < pixbuf_size; i++)
			if (inpic_cbuf_ptr[i] > gui_info.tf_u)
				inpic_cbuf_ptr[i] = 255;
			else if (inpic_cbuf_ptr[i] < gui_info.tf_l)
				inpic_cbuf_ptr[i] = 0;

	ipictxt = gdk_texture_new_for_pixbuf (inpic_cbuf);			// create texutre from the copy of pixel data
	gtk_picture_set_paintable (GTK_PICTURE(in_pic), GDK_PAINTABLE(ipictxt));
	gdk_texture_save_to_png (ipictxt, "cv.png");
	g_clear_object (&ipictxt);						// clear texture
}

/* CALLBACK: zoom on scroll */
void zoom_clb(	GtkEventControllerScroll* gesture,
		gdouble dx, gdouble dy,
		GtkWidget* in_pic )
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

	gtk_widget_set_size_request(in_pic, w * scale, h * scale);
	gtk_widget_queue_draw(in_pic);
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
			gtk_widget_queue_draw(in_pic);
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
void open_cb (GObject *oidiag, GAsyncResult *res, gpointer data) {
	GFile* inpic;
	inpic = gtk_file_dialog_open_finish (	GTK_FILE_DIALOG(oidiag),
						res,
						NULL );				// open file
	GdkTexture *ipictxt;							// inserted picture texture
	inpic_obuf = gdk_pixbuf_new_from_file (g_file_get_path(inpic), NULL);	// retrive pixel data from picture file
	ipictxt = gdk_texture_new_for_pixbuf (inpic_obuf);			// create texutre from pixel data
	gtk_picture_set_paintable (GTK_PICTURE(in_pic), GDK_PAINTABLE(ipictxt));
	g_clear_object (&ipictxt);						// clear texture
}

/* CALLBACK: click on insert picture */
void insrt_sub (GtkWidget* widget, GtkWindow* main_window) {
	GtkFileDialog* oidiag = gtk_file_dialog_new ();				// open image dialog
	gtk_file_dialog_set_modal (oidiag, TRUE);				// disable main window when choosing a new file

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
	gtk_file_dialog_set_filters(oidiag, G_LIST_MODEL(suffix_list));

	gtk_file_dialog_open (oidiag, main_window, NULL, &open_cb, NULL);	// open_cb will be called on open or cancel
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
	GtkWidget *mwin;							// main window
	GtkWidget *winpan;							// main pane container
	GtkWidget *cpan;							// control pane
	GtkWidget *popan;							// picture output pane
	GtkWidget *header;							// headerbar
	GtkWidget *open_btn_lbl, *open_btn_icn, *open_btn_box, *open_btn;	// insert picture button
	GtkWidget *tf_box, *tf_check, *tf_uval, *tf_lval;			// threshold filter

	/* create a new window, and set its title */
	mwin = gtk_application_window_new (app);
	gtk_window_set_title (GTK_WINDOW (mwin), "Window");

	/* headerbar*/
	header = gtk_header_bar_new ();
	gtk_window_set_titlebar (GTK_WINDOW (mwin), header);

	/* open button in headerbar */
	open_btn_icn = gtk_image_new_from_icon_name ("insert-image");		// icon for open button
	open_btn_lbl = gtk_label_new ("Insert");				// label for open button
	open_btn_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);		// container for icon + label
	open_btn = gtk_button_new ();						// open button itself

	gtk_box_append (GTK_BOX(open_btn_box), open_btn_icn);			// attach icon to the container
	gtk_box_append (GTK_BOX(open_btn_box), open_btn_lbl);			// attach label to the container
	gtk_button_set_child (GTK_BUTTON(open_btn), open_btn_box);		// attach container to open button

	gtk_box_set_homogeneous (GTK_BOX(open_btn_box), FALSE);			// container decoration
	gtk_label_set_justify (GTK_LABEL(open_btn_lbl), GTK_JUSTIFY_CENTER);

	g_signal_connect (open_btn, "clicked", G_CALLBACK (insrt_sub), mwin);	// click on insert button signal
	gtk_header_bar_pack_start (GTK_HEADER_BAR (header), open_btn);		// attach open button to the header bar

	/* main window layout */
	winpan = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);			// create paned view
	cpan = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);			// create control pane
	popan = gtk_scrolled_window_new ();					// create picture output pane
	in_pic = gtk_picture_new ();						// create picture object

	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW(popan), in_pic);	// attach picture to pane
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
	tf_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);			// threshold filter item
	tf_check = gtk_check_button_new_with_label ("threshold filter");	// threshold filter checkbox
	tf_uval = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL
						, 0, 255, 1);			// threshold filter upper value
	tf_lval = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL
						, 0, 255, 1);			// threshold filter upper value
	gtk_scale_set_draw_value (GTK_SCALE(tf_uval), TRUE);			// upper value slider decorations
	gtk_scale_set_draw_value (GTK_SCALE(tf_lval), TRUE);			// lower value slider decorations
	gtk_widget_set_sensitive (tf_uval, FALSE);				// deactivate upper value slider at first till tf_check
	gtk_widget_set_sensitive (tf_lval, FALSE);				// deactivate lower value slider at first till tf_check
	gtk_range_set_value (GTK_RANGE(tf_uval), gui_info.tf_u);		// threshold filter upper limit default value
	gtk_range_set_value (GTK_RANGE(tf_lval), gui_info.tf_l);		// threshold filter upper lower default value

	gtk_box_append (GTK_BOX(tf_box), tf_check);				// attach checkbox to threshold filter container
	gtk_box_append (GTK_BOX(tf_box), tf_uval);				// attach upper value slider to threshold filter container
	gtk_box_append (GTK_BOX(tf_box), tf_lval);				// attach lower value slider to threshold filter container
	gtk_box_append (GTK_BOX(cpan), tf_box);					// attach threshold filter item to control pane
	g_signal_connect (tf_uval, "value-changed", G_CALLBACK (tfu_clb), NULL);// attach threshold filter signal on upper limit value change
	g_signal_connect (tf_lval, "value-changed", G_CALLBACK (tfl_clb), NULL);// attach threshold filter signal on lower limit value change
	g_object_set_data (G_OBJECT(tf_check), "tf_uval", tf_uval);		// append tf_uval to tf_check and name it for later recovery
	g_object_set_data (G_OBJECT(tf_check), "tf_lval", tf_lval);		// and preventing globals
	g_signal_connect (tf_check, "toggled", G_CALLBACK (tf_st), tf_check);	// attach threshold filter signal on activation/deactivation

	/* render */
	gtk_window_present (GTK_WINDOW (mwin));

	/* events */
	// mouse scroll
	GtkEventController* scroll_event =
		gtk_event_controller_scroll_new (
			GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
			GTK_EVENT_CONTROLLER_SCROLL_DISCRETE
		);
	gtk_widget_add_controller(in_pic, scroll_event);
	g_signal_connect(scroll_event, "scroll", G_CALLBACK(zoom_clb), in_pic);
	// mouse click
	GtkGesture* click_event = gtk_gesture_click_new();
	gtk_gesture_single_set_button (
		GTK_GESTURE_SINGLE(click_event),
		GDK_BUTTON_PRIMARY
	);
	gtk_widget_add_controller(in_pic, GTK_EVENT_CONTROLLER(click_event));
	g_signal_connect(click_event, "pressed", G_CALLBACK(mcp_clb), in_pic);
	g_signal_connect(click_event, "released", G_CALLBACK(mcr_clb), in_pic);
	// mouse motion
	GtkEventController* mm_event = gtk_event_controller_motion_new();
	gtk_widget_add_controller(in_pic, mm_event);
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
