/* ide-terminal-popover.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-terminal-popover"

#include "config.h"

#include <dazzle.h>
#include <libide-foundry.h>
#include <libide-gui.h>

#include "ide-terminal-popover.h"
#include "ide-terminal-popover-row.h"

struct _IdeTerminalPopover
{
  GtkPopover          parent_instance;

  DzlListModelFilter *filter;
  gchar              *selected;

  /* Template widgets */
  GtkSearchEntry     *search_entry;
  GtkListBox         *list_box;
};

G_DEFINE_TYPE (IdeTerminalPopover, ide_terminal_popover, GTK_TYPE_POPOVER)

static void
ide_terminal_popover_update_selected_cb (GtkWidget *widget,
                                         gpointer   user_data)
{
  IdeTerminalPopoverRow *row = (IdeTerminalPopoverRow *)widget;
  IdeRuntime *runtime = user_data;
  gboolean selected;

  g_assert (IDE_IS_TERMINAL_POPOVER_ROW (row));
  g_assert (IDE_IS_RUNTIME (runtime));

  selected = runtime == ide_terminal_popover_row_get_runtime (row);
  ide_terminal_popover_row_set_selected (row, selected);
}

static void
ide_terminal_popover_row_activated_cb (IdeTerminalPopover    *self,
                                       IdeTerminalPopoverRow *row,
                                       GtkListBox            *list_box)
{
  IdeRuntime *runtime;

  g_assert (IDE_IS_TERMINAL_POPOVER (self));
  g_assert (IDE_IS_TERMINAL_POPOVER_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  runtime = ide_terminal_popover_row_get_runtime (row);

  g_free (self->selected);
  self->selected = g_strdup (ide_runtime_get_id (runtime));

  gtk_container_foreach (GTK_CONTAINER (self->list_box),
                         ide_terminal_popover_update_selected_cb,
                         runtime);
}

static GtkWidget *
ide_terminal_popover_create_row_cb (gpointer item,
                                    gpointer user_data)
{
  IdeRuntime *runtime = item;
  IdeTerminalPopover *self = user_data;
  GtkWidget *row = ide_terminal_popover_row_new (runtime);

  if (ide_str_equal0 (ide_runtime_get_id (runtime), self->selected))
    ide_terminal_popover_row_set_selected (IDE_TERMINAL_POPOVER_ROW (row), TRUE);
  else
    ide_terminal_popover_row_set_selected (IDE_TERMINAL_POPOVER_ROW (row), FALSE);

  return row;
}

static gboolean
ide_terminal_popover_filter_func (GObject  *item,
                                  gpointer  user_data)
{
  DzlPatternSpec *spec = user_data;
  IdeRuntime *runtime = IDE_RUNTIME (item);
  const gchar *str;

  str = ide_runtime_get_id (runtime);
  if (dzl_pattern_spec_match (spec, str))
    return TRUE;

  str = ide_runtime_get_category (runtime);
  if (dzl_pattern_spec_match (spec, str))
    return TRUE;

  str = ide_runtime_get_display_name (runtime);
  if (dzl_pattern_spec_match (spec, str))
    return TRUE;

  return FALSE;
}

static void
ide_terminal_popover_search_changed_cb (IdeTerminalPopover *self,
                                        GtkSearchEntry     *entry)
{
  const gchar *text;

  g_assert (IDE_IS_TERMINAL_POPOVER (self));
  g_assert (GTK_IS_SEARCH_ENTRY (entry));

  if (self->filter == NULL)
    return;

  text = gtk_entry_get_text (GTK_ENTRY (entry));
  if (ide_str_empty0 (text))
    text = NULL;

  dzl_list_model_filter_set_filter_func (self->filter,
                                         ide_terminal_popover_filter_func,
                                         dzl_pattern_spec_new (text),
                                         (GDestroyNotify) dzl_pattern_spec_unref);
}

static void
ide_terminal_popover_context_set_cb (GtkWidget  *widget,
                                     IdeContext *context)
{
  IdeTerminalPopover *self = (IdeTerminalPopover *)widget;
  IdeRuntimeManager *runtime_manager;
  IdeConfigManager *config_manager;
  IdeConfig *config;

  g_assert (IDE_IS_TERMINAL_POPOVER (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context == NULL)
    return;

  config_manager = ide_config_manager_from_context (context);
  config = ide_config_manager_get_current (config_manager);
  runtime_manager = ide_runtime_manager_from_context (context);

  if (config != NULL)
    {
      g_free (self->selected);
      self->selected = g_strdup (ide_config_get_runtime_id (config));
    }

  g_clear_object (&self->filter);
  self->filter = dzl_list_model_filter_new (G_LIST_MODEL (runtime_manager));

  gtk_list_box_bind_model (self->list_box,
                           G_LIST_MODEL (self->filter),
                           ide_terminal_popover_create_row_cb,
                           self, NULL);
}

static void
ide_terminal_popover_class_init (IdeTerminalPopoverClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-terminal/ui/ide-terminal-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalPopover, list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalPopover, search_entry);
}

static void
ide_terminal_popover_init (IdeTerminalPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->selected = g_strdup ("host");

  ide_widget_set_context_handler (self, ide_terminal_popover_context_set_cb);

  g_signal_connect_object (self->search_entry,
                           "changed",
                           G_CALLBACK (ide_terminal_popover_search_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->list_box,
                           "row-activated",
                           G_CALLBACK (ide_terminal_popover_row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

GtkWidget *
ide_terminal_popover_new (void)
{
  return g_object_new (IDE_TYPE_TERMINAL_POPOVER, NULL);
}

/**
 * ide_terminal_popover_get_runtime:
 * @self: a #IdeTerminalPopover
 *
 * Returns: (transfer none): an #IdeRuntime or %NULL
 *
 * Since: 3.32
 */
IdeRuntime *
ide_terminal_popover_get_runtime (IdeTerminalPopover *self)
{
  IdeRuntimeManager *runtime_manager;
  IdeContext *context;

  g_return_val_if_fail (IDE_IS_TERMINAL_POPOVER (self), NULL);

  if (self->selected != NULL &&
      (context = ide_widget_get_context (GTK_WIDGET (self))) &&
      (runtime_manager = ide_runtime_manager_from_context (context)))
    return ide_runtime_manager_get_runtime (runtime_manager, self->selected);

  return NULL;
}
