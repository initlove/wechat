#include <cairo.h>
#include <gtk/gtk.h>
#include <math.h>

#define TC 24
#define MID_X 400.0
#define MID_Y 350.0
#define MID_R 200.0

static int counts [24] = {
	598, 111, 32, 13, 35, 34,
	138, 609, 2215, 3835, 4554, 3173, 
	1704, 2294, 2995, 3097, 3753, 3048,
	2549, 1906, 2206, 2224, 2025, 1485
};

static void do_drawing(cairo_t *);

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, 
	  gpointer user_data)
{
	do_drawing(cr);

	return FALSE;
}

static float
get_red (int i)
{
	return 1.0 * counts[i] / 5000;
}

static void do_drawing(cairo_t *cr)
{
	double angle[TC + 1];
	double X[TC + 1];
	double Y[TC + 1];
	int i;
	
	double degree;
	for (i = 0; i < TC; i++) {
		degree = 360/TC * i;
		angle[i] = 1.0*(270 + degree) * (M_PI)/180.0;
		X[i] = MID_X + sin(degree * M_PI /180.0) * MID_R;
		Y[i] = MID_Y - cos(degree * M_PI /180.0) * MID_R;
	}

	for (i = 0; i < TC; i++) {
		float red, green;
		if (counts[i] > 2500) {
			red = 1.0;
			green = 1.0*(5000-counts[i]) /2500;
		} else {
			green = 1.0;
			red = 1.0*(counts[i]) / 2500;
		}
		cairo_set_source_rgba (cr, red, green, 0, 1);

		cairo_move_to (cr, MID_X, MID_Y);
		cairo_line_to (cr, X[i], Y[i]);
		cairo_arc (cr, MID_X, MID_Y, MID_R, angle[i], angle[(i+1)%TC]);
		cairo_line_to (cr, MID_X, MID_Y);
		cairo_fill (cr);
	}
	
	cairo_set_source_rgba (cr, 0, 0, 0, 1);
	cairo_set_line_width (cr, 1.0);
	for (i = 0; i < TC; i++) {
		cairo_move_to (cr, MID_X, MID_Y);
		cairo_line_to (cr, X[i], Y[i]);
		cairo_arc (cr, MID_X, MID_Y, MID_R, angle[i], angle[(i+1)%TC]);
		cairo_line_to (cr, MID_X, MID_Y);
		cairo_stroke (cr);
	}

	cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, 20.0);

	for (i = 0; i < TC; i++) {
		float x, y;
		gchar *hour;

		degree = 360/TC * i;
		x = MID_X + sin(degree * M_PI /180.0) * MID_R * 1.2 - 5;
		y = MID_Y - cos(degree * M_PI /180.0) * MID_R * 1.2 + 5;
		
		cairo_set_font_size (cr, 20.0);
		cairo_set_source_rgba (cr, 0, 0, 0, 1);
		cairo_move_to (cr, x, y);
		hour = g_strdup_printf ("%d", i);
		cairo_show_text (cr, hour);

		if (i == 8) {
			cairo_set_source_rgba (cr, 1, 0, 0, 1);
			cairo_set_font_size (cr, 15.0);
			cairo_move_to (cr, x + 20, y);
			cairo_show_text (cr, "开始上班");
		} else if (i == 13) {
			cairo_set_source_rgba (cr, 1, 0, 0, 1);
			cairo_set_font_size (cr, 15.0);
			cairo_move_to (cr, x + 5, y + 20);
			cairo_show_text (cr, "午饭时间");
		} else if (i == 19) {
			cairo_set_source_rgba (cr, 1, 0, 0, 1);
			cairo_set_font_size (cr, 15.0);
			cairo_move_to (cr, x - 10, y - 20);
			cairo_show_text (cr, "回家晚饭");
		}
	}

	gchar *note[6] = {"微信群统计", "数据来源：", "真实群一年来", "4万多条聊天记录", "绿色表示发言较少", "红色表示发言较多"};
	cairo_set_font_size (cr, 20);
	cairo_move_to (cr,  700, 180);
	cairo_show_text (cr, note[0]);
	for (i = 1; i < 6; i++) {
		cairo_move_to (cr,  700, 200 + i * 30);
		cairo_show_text (cr, note[i]);
	}
}

int main(int argc, char *argv[])
{
	GtkWidget *window;
	GtkWidget *darea;
	
	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	darea = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(window), darea);
 
	gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);

	g_signal_connect(G_OBJECT(darea), "draw", 
	    G_CALLBACK(on_draw_event), NULL); 
	g_signal_connect(window, "destroy",
	    G_CALLBACK(gtk_main_quit), NULL);  
	  
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(window), 1000, 800); 
	gtk_window_set_title(GTK_WINDOW(window), "微信群统计");

	gtk_widget_show_all(window);

	gtk_main();

	return 0;
}
