/* gbp-flatpak-runner.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "gbp-flatpak-runner"

#include <errno.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <unistd.h>

#include "gbp-flatpak-runner.h"
#include "gbp-flatpak-configuration.h"

struct _GbpFlatpakRunner
{
  IdeRunner parent_instance;

  gchar *build_path;
  gchar *binary_path;
};

G_DEFINE_TYPE (GbpFlatpakRunner, gbp_flatpak_runner, IDE_TYPE_RUNNER)

static IdeSubprocessLauncher *
gbp_flatpak_runner_create_launcher (IdeRunner *runner)
{
  return ide_subprocess_launcher_new (0);
}

static void
gbp_flatpak_runner_fixup_launcher (IdeRunner             *runner,
                                   IdeSubprocessLauncher *launcher)
{
  GbpFlatpakRunner *self = (GbpFlatpakRunner *)runner;
  IdeContext *context;
  IdeConfigurationManager *config_manager;
  IdeConfiguration *configuration;
  guint i = 0;

  g_assert (GBP_IS_FLATPAK_RUNNER (runner));
  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_context_get_configuration_manager (context);
  configuration = ide_configuration_manager_get_current (config_manager);

  ide_subprocess_launcher_insert_argv (launcher, i++, "flatpak");
  ide_subprocess_launcher_insert_argv (launcher, i++, "build");
  ide_subprocess_launcher_insert_argv (launcher, i++, "--allow=devel");

  if (GBP_IS_FLATPAK_CONFIGURATION (configuration))
    {
      const gchar * const *finish_args = gbp_flatpak_configuration_get_finish_args (GBP_FLATPAK_CONFIGURATION (configuration));

      for (guint j = 0; finish_args[j]; j++)
        {
          const gchar *arg = finish_args[j];

          /* "flatpak build" does not support require-version */
          if (!g_str_has_prefix (arg, "--require-version"))
            ide_subprocess_launcher_insert_argv (launcher, i++, arg);
        }
    }
  else
    {
      ide_subprocess_launcher_insert_argv (launcher, i++, "--share=ipc");
      ide_subprocess_launcher_insert_argv (launcher, i++, "--socket=x11");
      ide_subprocess_launcher_insert_argv (launcher, i++, "--socket=wayland");
    }

  ide_subprocess_launcher_insert_argv (launcher, i++, self->build_path);
}

GbpFlatpakRunner *
gbp_flatpak_runner_new (IdeContext  *context,
                        const gchar *build_path,
                        const gchar *binary_path)
{
  GbpFlatpakRunner *self;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  self = g_object_new (GBP_TYPE_FLATPAK_RUNNER,
                       "context", context,
                       NULL);

  ide_runner_append_argv (IDE_RUNNER (self), binary_path);

  self->build_path = g_strdup (build_path);
  self->binary_path = g_strdup (binary_path);

  return self;
}

static void
gbp_flatpak_runner_finalize (GObject *object)
{
  GbpFlatpakRunner *self = (GbpFlatpakRunner *)object;

  g_clear_pointer (&self->build_path, g_free);
  g_clear_pointer (&self->binary_path, g_free);

  G_OBJECT_CLASS (gbp_flatpak_runner_parent_class)->finalize (object);
}

static void
gbp_flatpak_runner_class_init (GbpFlatpakRunnerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeRunnerClass *runner_class = IDE_RUNNER_CLASS (klass);

  object_class->finalize = gbp_flatpak_runner_finalize;

  runner_class->create_launcher = gbp_flatpak_runner_create_launcher;
  runner_class->fixup_launcher = gbp_flatpak_runner_fixup_launcher;
}

static void
gbp_flatpak_runner_init (GbpFlatpakRunner *self)
{
  ide_runner_set_run_on_host (IDE_RUNNER (self), TRUE);
  ide_runner_set_clear_env (IDE_RUNNER (self), FALSE);
}
