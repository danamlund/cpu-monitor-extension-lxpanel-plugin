/**
 * CPU usage plugin to lxpanel
 *
 * Copyright (c) 2008 LxDE Developers, see the file AUTHORS for details.
 * Copyright (C) 2004 by Alexandre Pereira da Silva <alexandre.pereira@poli.usp.br>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
/*A little bug fixed by Mykola <mykola@2ka.mipt.ru>:) */
/* v. 1.2: Added settings to control which CPU to show statistics

   for and memory usage or swap usage. by dan@danamlund.dk */
/* v. 1.3: Added setting to control the width of the widget. */

/* v. 1.4: Added better makefile. */

/* v. 1.5: Enable modification of the background color via the settings dialog. */

/* 
sudo apt-get install build-essential lxde
sudo apt-get install libglib2.0-dev libmenu-cache1-dev libgtk2.0-dev
 */

#define VERSION "1.5"

#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#include "lxpanel/plugin.h"

/* from lxpanel-0.5.8/src/configurator.h */
GtkWidget* create_generic_config_dlg( const char* title, GtkWidget* parent,
                                      GSourceFunc apply_func, Plugin * plugin,
                                      const char* name, ... );

/* from lxpanel-0.5.8/src/plugin.h */
gboolean plugin_button_press_event(GtkWidget *widget, 
                                   GdkEventButton *event, Plugin *plugin);

/* from lxpanel-0.5.8/src/panel.h */
void panel_apply_icon(GtkWindow *w);

#define BORDER_SIZE 2
#define PANEL_HEIGHT_DEFAULT 25

typedef unsigned long CPUTick;			/* Value from /proc/stat */
typedef float CPUSample;			/* Saved CPU utilization value as 0.0..1.0 */

struct cpu_stat {
    CPUTick u, n, s, i;				/* User, nice, system, idle */
};

/* Private context for CPU plugin. */
typedef struct {
    GdkGC * graphics_context;			/* Graphics context for drawing area */
    GdkGC * background_gc;			/* Graphics context for background */
    GdkColor foreground_color;			/* Foreground color for drawing area */
    GdkColor background_color;			/* Foreground color for background */
    GtkWidget * da;				/* Drawing area */
    GdkPixmap * pixmap;				/* Pixmap to be drawn on drawing area */
 
    gchar * action;				/* Command to execute on a double click */
    guint timer;				/* Timer for periodic update */
    CPUSample * stats_cpu;			/* Ring buffer of CPU utilization values */
    unsigned int ring_cursor;			/* Cursor for ring buffer */
    int pixmap_width;				/* Width of drawing area pixmap; also size of ring buffer; does not include border size */
    int pixmap_height;				/* Height of drawing area pixmap; does not include border size */
    struct cpu_stat previous_cpu_stat;		/* Previous value of cpu_stat */
    int cpu_num;
    int widget_width;
    GtkWidget * config_dlg;
    GList *cpu_names;
  int cpu_amount;
  char *foreground_color_string;
  char *background_color_string;
  Plugin *p;
} CPUPlugin;

static void redraw_pixmap(CPUPlugin * c);
static int cpu_get_cpu_amount();
static gboolean cpu_get_info(int cpu_num, struct cpu_stat *cpu);
static gboolean cpu_update(CPUPlugin * c);
static gboolean configure_event(GtkWidget * widget, GdkEventConfigure * event, CPUPlugin * c);
static gboolean expose_event(GtkWidget * widget, GdkEventExpose * event, CPUPlugin * c);
static int cpu_constructor(Plugin * p, char ** fp);
static void cpu_destructor(Plugin * p);
static void cpu_apply_configuration(Plugin * p);
static void cpu_default_cpu_changed(GtkComboBox * cb, gpointer * data);
static void cpu_configure(Plugin * p, GtkWindow * parent);
static void cpu_save_configuration(Plugin * p, FILE * fp);
static void cpu_panel_configuration_changed(Plugin * p);

/* Redraw after timer callback or resize. */
static void redraw_pixmap(CPUPlugin * c)
{
  if (c->pixmap == NULL)
    return;

    /* Erase pixmap. */
    gdk_draw_rectangle(c->pixmap, c->background_gc, TRUE, 0, 0, c->pixmap_width, c->pixmap_height);

    /* Recompute pixmap. */
    unsigned int i;
    unsigned int drawing_cursor = c->ring_cursor;
    for (i = 0; i < c->pixmap_width; i++)
    {
        /* Draw one bar of the CPU usage graph. */
        if (c->stats_cpu[drawing_cursor] != 0.0)
            gdk_draw_line(c->pixmap, c->graphics_context,
                i, c->pixmap_height,
                i, c->pixmap_height - c->stats_cpu[drawing_cursor] * c->pixmap_height);

        /* Increment and wrap drawing cursor. */
        drawing_cursor += 1;
	if (drawing_cursor >= c->pixmap_width)
            drawing_cursor = 0;
    }

    /* Redraw pixmap. */
    gtk_widget_queue_draw(c->da);
}

static int cpu_get_cpu_amount() 
{
   int sscanf_result, cpu_amount = 0;
     char line[256], cpu_str[10];
     FILE * stat = fopen("/proc/stat", "r");
     if (stat == NULL)
        return 0;
     
     do {
       fgets(line, 100, stat);
       sscanf_result = sscanf(line, "%s ", cpu_str);
       if (sscanf_result) {}
       cpu_amount++;
     } while (strncmp(cpu_str, "cpu", 3) == 0);
  /* minus the first average CPU, and the extra from using do-while. */
  cpu_amount -= 2;

  return cpu_amount;
}

static gboolean cpu_get_info(int cpu_num, struct cpu_stat *cpu) 
{
  char line[256], cpu_str[10];
  int sscanf_result, i;

  FILE * stat = fopen("/proc/stat", "r");
  if (stat == NULL)
    return FALSE;

  /* Jump to defined cpu */
  for (i = 0; i <= cpu_num; i++) 
  {
    fgets(line, 100, stat);
  }
  line[strlen(line)-1] = '\0';
  /* Get CPU information */
  sscanf_result = sscanf(line, "%s %lu %lu %lu %lu", 
                         cpu_str, &cpu->u, &cpu->n, &cpu->s, &cpu->i);
  fclose(stat);
  /* Return true if the defined CPU exists */
  return sscanf_result == 5 && strncmp(cpu_str, "cpu", 3) == 0;
}

static gboolean cpu_get_info_mem(CPUPlugin *c) {
  FILE *meminfo = fopen("/proc/meminfo", "r");
  if (meminfo == NULL)
    return FALSE;

  int memtotal, memfree, buffers, cached, swaptotal, swapfree;
  memtotal = memfree = buffers = cached = swaptotal = swapfree = 0;
  char line[256];
  while (NULL != fgets(line, 256, meminfo)) {
    sscanf(line, "MemTotal: %d", &memtotal);
    sscanf(line, "MemFree: %d", &memfree);
    sscanf(line, "Buffers: %d", &buffers);
    sscanf(line, "Cached: %d", &cached);
    sscanf(line, "SwapTotal: %d", &swaptotal);
    sscanf(line, "SwapFree: %d", &swapfree);
  }
  fclose(meminfo);
  
  float stat = 0.0;
  if (c->cpu_num == c->cpu_amount + 1) { /* mem free */
    stat = (float) 1 - (float) (memfree + buffers + cached) / memtotal;
  } else {                      /* swap free */
    stat = (float) 1 - (float) swapfree / swaptotal;
  }

  c->stats_cpu[c->ring_cursor] = stat;
  c->ring_cursor += 1;
  if (c->ring_cursor >= c->pixmap_width)
    c->ring_cursor = 0;

  return TRUE;
}

/* Periodic timer callback. */
static gboolean cpu_update(CPUPlugin * c)
{
    if ((c->stats_cpu != NULL) && (c->pixmap != NULL))
    {
      if (c->cpu_num > c->cpu_amount) {
        cpu_get_info_mem(c);
        redraw_pixmap(c);
        return TRUE;
      }
        /* Open statistics file and scan out CPU usage. */
        struct cpu_stat cpu;
        
        if ( cpu_get_info(c->cpu_num, &cpu) )
        {

            /* Compute delta from previous statistics. */
            struct cpu_stat cpu_delta;
            cpu_delta.u = cpu.u - c->previous_cpu_stat.u;
            cpu_delta.n = cpu.n - c->previous_cpu_stat.n;
            cpu_delta.s = cpu.s - c->previous_cpu_stat.s;
            cpu_delta.i = cpu.i - c->previous_cpu_stat.i;

            /* Copy current to previous. */
            memcpy(&c->previous_cpu_stat, &cpu, sizeof(struct cpu_stat));

            /* Compute user+nice+system as a fraction of total.
             * Introduce this sample to ring buffer, increment and wrap ring buffer cursor. */
            float cpu_uns = cpu_delta.u + cpu_delta.n + cpu_delta.s;
            c->stats_cpu[c->ring_cursor] = cpu_uns / (cpu_uns + cpu_delta.i);
            c->ring_cursor += 1;
            if (c->ring_cursor >= c->pixmap_width)
                c->ring_cursor = 0;

            /* Redraw with the new sample. */
            redraw_pixmap(c);
        }
    }
    return TRUE;
}

/* Handler for configure_event on drawing area. */
static gboolean configure_event(GtkWidget * widget, GdkEventConfigure * event, CPUPlugin * c)
{
    /* Allocate pixmap and statistics buffer without border pixels. */
    int new_pixmap_width = widget->allocation.width - BORDER_SIZE * 2;
    int new_pixmap_height = widget->allocation.height - BORDER_SIZE * 2;
    if ((new_pixmap_width > 0) && (new_pixmap_height > 0))
    {
        /* If statistics buffer does not exist or it changed size, reallocate and preserve existing data. */
        if ((c->stats_cpu == NULL) || (new_pixmap_width != c->pixmap_width))
        {
            CPUSample * new_stats_cpu = g_new0(typeof(*c->stats_cpu), new_pixmap_width);
            if (c->stats_cpu != NULL)
            {
                if (new_pixmap_width > c->pixmap_width)
                {
                    /* New allocation is larger.
                     * Introduce new "oldest" samples of zero following the cursor. */
                    memcpy(&new_stats_cpu[0],
                        &c->stats_cpu[0], c->ring_cursor * sizeof(CPUSample));
                    memcpy(&new_stats_cpu[new_pixmap_width - c->pixmap_width + c->ring_cursor],
                        &c->stats_cpu[c->ring_cursor], (c->pixmap_width - c->ring_cursor) * sizeof(CPUSample));
                }
                else if (c->ring_cursor <= new_pixmap_width)
                {
                    /* New allocation is smaller, but still larger than the ring buffer cursor.
                     * Discard the oldest samples following the cursor. */
                    memcpy(&new_stats_cpu[0],
                        &c->stats_cpu[0], c->ring_cursor * sizeof(CPUSample));
                    memcpy(&new_stats_cpu[c->ring_cursor],
                        &c->stats_cpu[c->pixmap_width - new_pixmap_width + c->ring_cursor], (new_pixmap_width - c->ring_cursor) * sizeof(CPUSample));
                }
                else
                {
                    /* New allocation is smaller, and also smaller than the ring buffer cursor.
                     * Discard all oldest samples following the ring buffer cursor and additional samples at the beginning of the buffer. */
                    memcpy(&new_stats_cpu[0],
                        &c->stats_cpu[c->ring_cursor - new_pixmap_width], new_pixmap_width * sizeof(CPUSample));
                    c->ring_cursor = 0;
                }
                g_free(c->stats_cpu);
            }
            c->stats_cpu = new_stats_cpu;
        }

        /* Allocate or reallocate pixmap. */
        c->pixmap_width = new_pixmap_width;
        c->pixmap_height = new_pixmap_height;
        if (c->pixmap)
            g_object_unref(c->pixmap);
        c->pixmap = gdk_pixmap_new(widget->window, c->pixmap_width, c->pixmap_height, -1);

        /* Redraw pixmap at the new size. */
        redraw_pixmap(c);
    }
    return TRUE;
}

/* Handler for expose_event on drawing area. */
static gboolean expose_event(GtkWidget * widget, GdkEventExpose * event, CPUPlugin * c)
{
    /* Draw the requested part of the pixmap onto the drawing area.
     * Translate it in both x and y by the border size. */
    if (c->pixmap != NULL)
    {
        gdk_draw_drawable (widget->window,
              c->da->style->black_gc,
              c->pixmap,
              event->area.x, event->area.y,
              event->area.x + BORDER_SIZE, event->area.y + BORDER_SIZE,
              event->area.width, event->area.height);
    }
    return FALSE;
}

/* Handler for "button-press-event" event from main widget. */
static gboolean cpu_button_press_event(GtkWidget * widget, 
                                       GdkEventButton * evt, 
                                       Plugin * plugin)
{
    CPUPlugin * c = (CPUPlugin *) plugin->priv;

    /* Standard right-click handling. */
    if (plugin_button_press_event(widget, evt, plugin))
        return TRUE;

    if (NULL != c->action)
      g_spawn_command_line_async(c->action, NULL);

    return TRUE;
}

static void set_plot_color(CPUPlugin * c) {
  Plugin * p = c->p;
  if (c->graphics_context != NULL)
    g_object_unref(c->graphics_context);
  if (c->background_gc != NULL)
    g_object_unref(c->background_gc);

  c->graphics_context = gdk_gc_new(p->panel->topgwin->window);
  c->background_gc = gdk_gc_new(p->panel->topgwin->window);
  gdk_color_parse(c->foreground_color_string,  &c->foreground_color);
  gdk_color_parse(c->background_color_string,  &c->background_color);
  gdk_colormap_alloc_color
    (gdk_drawable_get_colormap(p->panel->topgwin->window),
     &c->foreground_color, FALSE, TRUE);
  gdk_colormap_alloc_color
    (gdk_drawable_get_colormap(p->panel->topgwin->window),
     &c->background_color, FALSE, TRUE);
  gdk_gc_set_foreground(c->graphics_context, &c->foreground_color);
  gdk_gc_set_foreground(c->background_gc, &c->background_color);
  redraw_pixmap(c);
}

/* Plugin constructor. */
static int cpu_constructor(Plugin * p, char ** fp)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    CPUPlugin * c = g_new0(CPUPlugin, 1);
    p->priv = c;
    c->cpu_num = 0;
    c->widget_width = 40;
    c->action = g_strdup("lxtask");

    c->p = p;
    c->cpu_amount = cpu_get_cpu_amount();

    line s;
    s.len = 256;
    if (fp != NULL)
    {
      while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END) 
      {
        if (s.type == LINE_NONE) 
        {
          ERR( "cpu: illegal token %s\n", s.str);
          return 0;
        }
        if (s.type == LINE_VAR) 
        {
          if (g_ascii_strcasecmp(s.t[0], "CPUNum") == 0)
            c->cpu_num = atoi(s.t[1]);
          else if (g_ascii_strcasecmp(s.t[0], "Color") == 0)
            c->foreground_color_string = g_strdup(s.t[1]); 
          else if (g_ascii_strcasecmp(s.t[0], "Background") == 0)
            c->background_color_string = g_strdup(s.t[1]);
          else if (g_ascii_strcasecmp(s.t[0], "Action") == 0) {
            g_free(c->action);
            c->action = g_strdup(s.t[1]);
          } else if (g_ascii_strcasecmp(s.t[0], "WidgetWidth") == 0) {
            c->widget_width = atoi(s.t[1]);
          } else
            ERR( "cpu: unknown var %s\n", s.t[0]);
        } 
        else 
        {
          ERR( "cpu: illegal in this context %s\n", s.str);
          return 0;
        }
      }
    }

    if (c->foreground_color_string == NULL)
      c->foreground_color_string = g_strdup("green");
    if (c->background_color_string == NULL)
      c->background_color_string = g_strdup("#000000");

    /* Allocate top level widget and set into Plugin widget pointer. */
    p->pwid = gtk_event_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(p->pwid), 1);
    GTK_WIDGET_SET_FLAGS(p->pwid, GTK_NO_WINDOW);

    /* Allocate drawing area as a child of top level widget.  Enable
       button press events. */
    c->da = gtk_drawing_area_new();
    gtk_widget_set_size_request(c->da, c->widget_width, PANEL_HEIGHT_DEFAULT);
    gtk_widget_add_events(c->da, GDK_BUTTON_PRESS_MASK);
    gtk_container_add(GTK_CONTAINER(p->pwid), c->da);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(c->da), "configure_event", 
                     G_CALLBACK(configure_event), (gpointer) c);
    g_signal_connect(G_OBJECT(c->da), "expose_event", 
                     G_CALLBACK(expose_event), (gpointer) c);
    g_signal_connect(G_OBJECT (p->pwid), "button_press_event", 
                     G_CALLBACK(cpu_button_press_event), (gpointer) p);
    /* g_signal_connect(c->da, "button-press-event",  */
    /*                  G_CALLBACK(plugin_button_press_event), p); */

    /* Show the widget.  Connect a timer to refresh the statistics. */
    gtk_widget_show(c->da);
    set_plot_color(c);

    c->timer = g_timeout_add(1500, (GSourceFunc) cpu_update, (gpointer) c);

    return 1;
}

/* Plugin destructor. */
static void cpu_destructor(Plugin * p)
{
    CPUPlugin * c = (CPUPlugin *) p->priv;

    /* Disconnect the timer. */
    g_source_remove(c->timer);

    /* Deallocate memory. */
    g_object_unref(c->graphics_context);
    g_object_unref(c->pixmap);
    g_free(c->stats_cpu);
    g_list_free(c->cpu_names);
    g_free(c->action);
    g_free(c->foreground_color_string);
    g_free(c);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static void cpu_apply_configuration(Plugin * p)
{
  CPUPlugin * c = (CPUPlugin *) p->priv;

  if (c->widget_width < 5)
    c->widget_width = 5;

  gtk_widget_set_size_request(c->da, c->widget_width, PANEL_HEIGHT_DEFAULT);

  if (c->stats_cpu != NULL) 
  {
    int i;
    for (i = 0; i < c->pixmap_width; i++) 
    {
      c->stats_cpu[i] = 0.0;
    }
    /* redraw_pixmap(c); */
  }
  set_plot_color(c);
}

/* Handler for "changed" event on default cpus combo box of
   configuration dialog. */
static void cpu_default_cpu_changed(GtkComboBox * cb, gpointer * data)
{
  /* Fetch the new value and redraw. */
  Plugin * p = (Plugin *) data;
  CPUPlugin * c = (CPUPlugin *) p->priv;

  c->cpu_num = gtk_combo_box_get_active(cb);
}

/* Callback when the configuration dialog is to be shown. */
static void cpu_configure(Plugin * p, GtkWindow * parent)
{
    CPUPlugin * c = (CPUPlugin *) p->priv;

    GtkWidget *dialog = create_generic_config_dlg
      (_(p->class->name),
       GTK_WIDGET(parent),
       (GSourceFunc) cpu_apply_configuration, (gpointer) p,
       _("Action on click"), &c->action, CONF_TYPE_STR,
       _("Color"), &c->foreground_color_string, CONF_TYPE_STR,
       _("Background"), &c->background_color_string, CONF_TYPE_STR,
       _("Width"), &c->widget_width, CONF_TYPE_INT,
       NULL);


    c->config_dlg = dialog;

    /* Create a vertical box as the child of the dialog. */
    GtkWidget * hbox = gtk_hbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);
  
    GtkWidget * label = gtk_label_new("CPU to monitor:");
    gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 2 );

    /* Create a combo box as the child of the horizontal box. */
    GtkWidget * combo = gtk_combo_box_new_text();
    gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, TRUE, 2);

    /* Populate the combo box with the available choices. */
    if (c->cpu_names == NULL) 
    {
      c->cpu_names = g_list_append(c->cpu_names, "Average");
      int i;
      char *cpu_name;
      for (i = 1; i <= c->cpu_amount; i++) 
      {
        cpu_name = g_new0(char, 10);
        sprintf(cpu_name, "CPU %d", i);
        c->cpu_names = g_list_append(c->cpu_names, cpu_name);
      }
      c->cpu_names = g_list_append(c->cpu_names, g_strdup("Memory usage"));
      c->cpu_names = g_list_append(c->cpu_names, g_strdup("Swap usage"));
    }

    GList *it;
    it = c->cpu_names;
    for (it = c->cpu_names; it; it = it->next) 
    {
      gtk_combo_box_append_text(GTK_COMBO_BOX(combo), (gchar*) it->data);
    }

    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), c->cpu_num);
    g_signal_connect(combo, "changed", G_CALLBACK(cpu_default_cpu_changed), p);

    gtk_widget_show_all(dialog);
    gtk_window_present(GTK_WINDOW(dialog));
}

/* Callback when the configuration is to be saved. */
static void cpu_save_configuration(Plugin * p, FILE * fp)
{
    CPUPlugin * c = (CPUPlugin *) p->priv;
    lxpanel_put_int(fp, "CPUNum", c->cpu_num);
    lxpanel_put_str(fp, "Action", c->action);
    lxpanel_put_str(fp, "Color", c->foreground_color_string);
    lxpanel_put_str(fp, "Background", c->background_color_string);
    lxpanel_put_int(fp, "WidgetWidth", c->widget_width);
}

/* Callback when panel configuration changes. */
static void cpu_panel_configuration_changed(Plugin * p)
{
    cpu_apply_configuration(p);
}

/* Plugin descriptor. */
PluginClass cpuda_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "cpuda",
    name : N_("CPU Usage Monitor (da)"),
    version: VERSION,
    description : N_("Display CPU usage"),

    constructor : cpu_constructor,
    destructor  : cpu_destructor,
    config : cpu_configure,
    save : cpu_save_configuration,
    panel_configuration_changed : cpu_panel_configuration_changed
};
