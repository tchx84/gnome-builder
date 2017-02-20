/* gbp-flatpak-configuration.c
 *
 * Copyright (C) 2016 Matthew Leeds <mleeds@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-configuration"

#include "gbp-flatpak-configuration.h"
#include "gbp-flatpak-runtime.h"

struct _GbpFlatpakConfiguration
{
  IdeConfiguration parent_instance;

  gchar  *branch;
  gchar  *command;
  gchar **finish_args;
  GFile  *manifest;
  gchar  *platform;
  gchar  *primary_module;
  gchar  *sdk;
};

G_DEFINE_TYPE (GbpFlatpakConfiguration, gbp_flatpak_configuration, IDE_TYPE_CONFIGURATION)

enum {
  PROP_0,
  PROP_BRANCH,
  PROP_COMMAND,
  PROP_FINISH_ARGS,
  PROP_MANIFEST,
  PROP_PLATFORM,
  PROP_PRIMARY_MODULE,
  PROP_SDK,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

JsonNode *
guess_primary_module (JsonNode   *modules_node,
                      IdeContext *context)
{
  JsonArray *modules;
  JsonNode *module;
  JsonNode *parent;
  GFile *project_file;
  g_autofree gchar *dir_name = NULL;
  g_autofree gchar *project_file_path = NULL;

  g_assert (IDE_IS_CONTEXT (context));
  g_return_val_if_fail (JSON_NODE_HOLDS_ARRAY (modules_node), NULL);

  /* Find the project directory name */
  project_file = ide_context_get_project_file (context);
  project_file_path = g_file_get_path (project_file);
  if (g_file_test (project_file_path, G_FILE_TEST_IS_DIR))
    {
      dir_name = g_file_get_basename (project_file);
    }
  else
    {
      g_autoptr(GFile) project_dir = NULL;
      project_dir = g_file_get_parent (project_file);
      dir_name = g_file_get_basename (project_dir);
    }
  g_assert (!ide_str_empty0 (dir_name));

  /* TODO: Support module strings that refer to other files? */
  modules = json_node_get_array (modules_node);
  if (json_array_get_length (modules) == 1)
    {
      module = json_array_get_element (modules, 0);
      if (JSON_NODE_HOLDS_OBJECT (module))
        return module;
    }
  else
    {
      for (guint i = 0; i < json_array_get_length (modules); i++)
        {
          module = json_array_get_element (modules, i);
          if (JSON_NODE_HOLDS_OBJECT (module))
            {
              const gchar *module_name;
              module_name = json_object_get_string_member (json_node_get_object (module), "name");
              if (g_strcmp0 (module_name, dir_name) == 0)
                return module;
              if (json_object_has_member (json_node_get_object (module), "modules"))
                {
                  JsonNode *nested_modules_node;
                  JsonNode *nested_primary_module;
                  nested_modules_node = json_object_get_member (json_node_get_object (module), "modules");
                  nested_primary_module = guess_primary_module (nested_modules_node, context);
                  if (nested_primary_module != NULL)
                    return nested_primary_module;
                }
            }
        }
        /* If none match, assume the last module in the list is the primary one */
        parent = json_node_get_parent (modules_node);
        if (JSON_NODE_HOLDS_OBJECT (parent) &&
            json_node_get_parent (parent) == NULL &&
            json_array_get_length (modules) > 0)
          {
            JsonNode *last_node;
            last_node = json_array_get_element (modules, json_array_get_length (modules) - 1);
            if (JSON_NODE_HOLDS_OBJECT (last_node))
              return last_node;
          }
    }

  return NULL;
}

/**
 * gbp_flatpak_configuration_load_from_file:
 * @manifest: A #GFile that might contain a flatpak manifest
 * @context: An #IdeContext
 *
 * This function attempts to parse the @manifest and use it to
 * populate the properties of a new #GbpFlatpakConfiguration.
 *
 * Returns: (transfer full): a #GbpFlatpakConfiguration if the file was
 * parsed successfully, otherwise %NULL
 */
GbpFlatpakConfiguration *
gbp_flatpak_configuration_load_from_file (GFile      *manifest,
                                          IdeContext *context)
{
  GbpFlatpakConfiguration *config;
  IdeEnvironment *environment = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *filename = NULL;
  g_autofree gchar *id = NULL;
  g_autofree gchar *runtime_id = NULL;
  const gchar *platform;
  const gchar *branch;
  const gchar *arch;
  const gchar *prefix;
  const gchar *sdk;
  const gchar *command = NULL;
  const gchar *app_id;
  const gchar *config_opts = NULL;
  const gchar *primary_module = NULL;
  g_auto(GStrv) finish_args = NULL;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) local_error = NULL;
  JsonNode *root_node = NULL;
  JsonNode *app_id_node = NULL;
  JsonNode *id_node = NULL;
  JsonNode *runtime_node = NULL;
  JsonNode *runtime_version_node = NULL;
  JsonNode *sdk_node = NULL;
  JsonNode *modules_node = NULL;
  JsonNode *primary_module_node = NULL;
  JsonNode *command_node = NULL;
  JsonNode *finish_args_node = NULL;
  JsonObject *root_object = NULL;

  g_assert (G_IS_FILE (manifest));
  g_assert (IDE_IS_CONTEXT (context));

  path = g_file_get_path (manifest);
  parser = json_parser_new ();
  if (!json_parser_load_from_file (parser, path, &local_error))
    {
      g_warning ("Error parsing potential flatpak manifest %s: %s", path, local_error->message);
      return NULL;
    }

  root_node = json_parser_get_root (parser);
  if (!JSON_NODE_HOLDS_OBJECT (root_node))
    return NULL;

  root_object = json_node_get_object (root_node);
  app_id_node = json_object_get_member (root_object, "app-id");
  id_node = json_object_get_member (root_object, "id");
  runtime_node = json_object_get_member (root_object, "runtime");
  runtime_version_node = json_object_get_member (root_object, "runtime-version");
  sdk_node = json_object_get_member (root_object, "sdk");
  modules_node = json_object_get_member (root_object, "modules");

  if ((!JSON_NODE_HOLDS_VALUE (app_id_node) && !JSON_NODE_HOLDS_VALUE (id_node)) ||
       !JSON_NODE_HOLDS_VALUE (runtime_node) ||
       !JSON_NODE_HOLDS_VALUE (sdk_node) ||
       !JSON_NODE_HOLDS_ARRAY (modules_node))
    return NULL;

  IDE_TRACE_MSG ("Discovered flatpak manifest at %s", path);

  /**
   * TODO: Currently we just support the build-options object that's global to the
   * manifest, but modules can have their own build-options as well that override
   * global ones, so we should consider supporting that. The main difficulty would
   * be keeping track of each so they can be written back to the file properly when
   * the user makes changes in the Builder interface.
   */
  if (json_object_has_member (root_object, "build-options") &&
      JSON_NODE_HOLDS_OBJECT (json_object_get_member (root_object, "build-options")))
    {
      JsonObject *build_options = NULL;

      build_options = json_object_get_object_member (root_object, "build-options");

      if (json_object_has_member (build_options, "prefix"))
        prefix = json_object_get_string_member (build_options, "prefix");

      environment = ide_environment_new ();
      if (json_object_has_member (build_options, "cflags"))
        {
          const gchar *cflags;
          cflags = json_object_get_string_member (build_options, "cflags");
          if (cflags != NULL)
            ide_environment_setenv (environment, "CFLAGS", cflags);
        }
      if (json_object_has_member (build_options, "cxxflags"))
        {
          const gchar *cxxflags;
          cxxflags = json_object_get_string_member (build_options, "cxxflags");
          if (cxxflags != NULL)
            ide_environment_setenv (environment, "CXXFLAGS", cxxflags);
        }
      if (json_object_has_member (build_options, "env"))
        {
          JsonObject *env_vars;
          env_vars = json_object_get_object_member (build_options, "env");
          if (env_vars != NULL)
            {
              g_autoptr(GList) env_list = NULL;
              GList *l;
              env_list = json_object_get_members (env_vars);
              for (l = env_list; l != NULL; l = l->next)
                {
                  const gchar *env_name = (gchar *)l->data;
                  const gchar *env_value = json_object_get_string_member (env_vars, env_name);
                  if (!ide_str_empty0 (env_name) && !ide_str_empty0 (env_value))
                    ide_environment_setenv (environment, env_name, env_value);
                }
            }
        }
    }

  platform = json_node_get_string (runtime_node);

  if (!JSON_NODE_HOLDS_VALUE (runtime_version_node) || ide_str_empty0 (json_node_get_string (runtime_version_node)))
    branch = "master";
  else
    branch = json_node_get_string (runtime_version_node);

  arch = flatpak_get_default_arch ();
  runtime_id = g_strdup_printf ("flatpak:%s/%s/%s", platform, arch, branch);

  sdk = json_node_get_string (sdk_node);

  command_node = json_object_get_member (root_object, "command");
  if (JSON_NODE_HOLDS_VALUE (command_node))
    command = json_node_get_string (command_node);

  finish_args_node = json_object_get_member (root_object, "finish-args");
  if (JSON_NODE_HOLDS_ARRAY (finish_args_node))
    {
      JsonArray *finish_args_json_array;
      GPtrArray *finish_args_ptr_array = NULL;
      finish_args_ptr_array = g_ptr_array_new ();
      finish_args_json_array = json_node_get_array (finish_args_node);
      for (guint i = 0; i < json_array_get_length (finish_args_json_array); i++)
        {
          const gchar *arg = json_array_get_string_element (finish_args_json_array, i);
          if (!ide_str_empty0 (arg))
            g_ptr_array_add (finish_args_ptr_array, g_strdup (arg));
        }
      g_ptr_array_add (finish_args_ptr_array, NULL);
      finish_args = (gchar **)g_ptr_array_free (finish_args_ptr_array, FALSE);
    }

  if (JSON_NODE_HOLDS_VALUE (app_id_node))
    app_id = json_node_get_string (app_id_node);
  else
    app_id = json_node_get_string (id_node);

  primary_module_node = guess_primary_module (modules_node, context);
  if (primary_module_node != NULL && JSON_NODE_HOLDS_OBJECT (primary_module_node))
    {
      JsonObject *primary_module_object = json_node_get_object (primary_module_node);
      primary_module = json_object_get_string_member (primary_module_object, "name");
      if (json_object_has_member (primary_module_object, "config-opts"))
        {
          JsonArray *config_opts_array;
          config_opts_array = json_object_get_array_member (primary_module_object, "config-opts");
          if (config_opts_array != NULL)
            {
              GPtrArray *config_opts_strv;
              config_opts_strv = g_ptr_array_new_with_free_func (g_free);
              for (guint i = 0; i < json_array_get_length (config_opts_array); i++)
                {
                  const gchar *next_option;
                  next_option = json_array_get_string_element (config_opts_array, i);
                  g_ptr_array_add (config_opts_strv, g_strdup (next_option));
                }
              g_ptr_array_add (config_opts_strv, NULL);
              config_opts = g_strjoinv (" ", (gchar **)config_opts_strv->pdata);
              g_ptr_array_free (config_opts_strv, TRUE);
            }
        }
    }

  /* Construct a config id using a hash of the file's content */
  {
    g_autofree gchar *manifest_data = NULL;
    g_autofree gchar *hash = NULL;
    gsize manifest_data_len = 0;

    filename = g_file_get_basename (manifest);

    if (g_file_get_contents (path, &manifest_data, &manifest_data_len, NULL))
      {
        g_autoptr(GChecksum) checksum = NULL;

        checksum = g_checksum_new (G_CHECKSUM_SHA1);
        g_checksum_update (checksum, (const guint8 *)manifest_data, manifest_data_len);
        hash = g_strdup (g_checksum_get_string (checksum));
      }

    if (hash != NULL)
      id = g_strdup_printf ("%s@%s", filename, hash);
    else
      id = g_strdup (filename);
  }

  config = g_object_new (GBP_TYPE_FLATPAK_CONFIGURATION,
                         "app-id", app_id,
                         "branch", branch,
                         "context", context,
                         "device-id", "local",
                         "display-name", filename,
                         "id", id,
                         "manifest", manifest,
                         "platform", platform,
                         "prefix", (prefix != NULL ? prefix : "/app"),
                         "runtime-id", runtime_id,
                         "sdk", sdk,
                         NULL);

  if (primary_module != NULL)
    gbp_flatpak_configuration_set_primary_module (config, primary_module);
  if (command != NULL)
    gbp_flatpak_configuration_set_command (config, command);
  if (finish_args != NULL)
    gbp_flatpak_configuration_set_finish_args (config, (const gchar * const *)finish_args);
  if (environment != NULL)
    ide_configuration_set_environment (IDE_CONFIGURATION (config), environment);
  if (config_opts != NULL)
    ide_configuration_set_config_opts (IDE_CONFIGURATION (config), config_opts);

  return config;
}

const gchar *
gbp_flatpak_configuration_get_branch (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return self->branch;
}

void
gbp_flatpak_configuration_set_branch (GbpFlatpakConfiguration *self,
                                      const gchar             *branch)
{
  g_return_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self));

  g_free (self->branch);
  self->branch = g_strdup (branch);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BRANCH]);
}

const gchar *
gbp_flatpak_configuration_get_command (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return self->command;
}

void
gbp_flatpak_configuration_set_command (GbpFlatpakConfiguration *self,
                                       const gchar             *command)
{
  g_return_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self));

  g_free (self->command);
  self->command = g_strdup (command);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_COMMAND]);
}

const gchar * const *
gbp_flatpak_configuration_get_finish_args (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return (const gchar * const *)self->finish_args;
}

void
gbp_flatpak_configuration_set_finish_args (GbpFlatpakConfiguration *self,
                                           const gchar * const     *finish_args)
{
  g_return_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self));

  if (self->finish_args != (gchar **)finish_args)
    {
      g_strfreev (self->finish_args);
      self->finish_args = g_strdupv ((gchar **)finish_args);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FINISH_ARGS]);
    }
}

gchar *
gbp_flatpak_configuration_get_manifest_path (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  if (self->manifest != NULL)
    return g_file_get_path (self->manifest);

  return NULL;
}

GFile *
gbp_flatpak_configuration_get_manifest (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return self->manifest;
}

const gchar *
gbp_flatpak_configuration_get_platform (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return self->platform;
}

void
gbp_flatpak_configuration_set_platform (GbpFlatpakConfiguration *self,
                                        const gchar             *platform)
{
  g_return_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self));

  g_free (self->platform);
  self->platform = g_strdup (platform);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PLATFORM]);
}

const gchar *
gbp_flatpak_configuration_get_primary_module (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return self->primary_module;
}

void
gbp_flatpak_configuration_set_primary_module (GbpFlatpakConfiguration *self,
                                              const gchar             *primary_module)
{
  g_return_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self));

  g_free (self->primary_module);
  self->primary_module = g_strdup (primary_module);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PRIMARY_MODULE]);
}

const gchar *
gbp_flatpak_configuration_get_sdk (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return self->sdk;
}

void
gbp_flatpak_configuration_set_sdk (GbpFlatpakConfiguration *self,
                                   const gchar             *sdk)
{
  g_return_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self));

  if (g_strcmp0 (self->sdk, sdk) != 0)
    {
      g_free (self->sdk);
      self->sdk = g_strdup (sdk);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SDK]);
    }
}

static gboolean
gbp_flatpak_configuration_supports_runtime (IdeConfiguration *configuration,
                                            IdeRuntime       *runtime)
{
  g_assert (GBP_IS_FLATPAK_CONFIGURATION (configuration));
  g_assert (IDE_IS_RUNTIME (runtime));

  return GBP_IS_FLATPAK_RUNTIME (runtime);
}

static void
gbp_flatpak_configuration_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbpFlatpakConfiguration *self = GBP_FLATPAK_CONFIGURATION (object);

  switch (prop_id)
    {
    case PROP_BRANCH:
      g_value_set_string (value, gbp_flatpak_configuration_get_branch (self));
      break;

    case PROP_COMMAND:
      g_value_set_string (value, gbp_flatpak_configuration_get_command (self));
      break;

    case PROP_FINISH_ARGS:
      g_value_set_boxed (value, gbp_flatpak_configuration_get_finish_args (self));
      break;

    case PROP_MANIFEST:
      g_value_set_object (value, gbp_flatpak_configuration_get_manifest (self));
      break;

    case PROP_PLATFORM:
      g_value_set_string (value, gbp_flatpak_configuration_get_platform (self));
      break;

    case PROP_PRIMARY_MODULE:
      g_value_set_string (value, gbp_flatpak_configuration_get_primary_module (self));
      break;

    case PROP_SDK:
      g_value_set_string (value, gbp_flatpak_configuration_get_sdk (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_flatpak_configuration_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GbpFlatpakConfiguration *self = GBP_FLATPAK_CONFIGURATION (object);

  switch (prop_id)
    {
    case PROP_BRANCH:
      gbp_flatpak_configuration_set_branch (self, g_value_get_string (value));
      break;

    case PROP_COMMAND:
      gbp_flatpak_configuration_set_command (self, g_value_get_string (value));
      break;

    case PROP_FINISH_ARGS:
      gbp_flatpak_configuration_set_finish_args (self, g_value_get_boxed (value));
      break;

    case PROP_MANIFEST:
      self->manifest = g_value_dup_object (value);
      break;

    case PROP_PLATFORM:
      gbp_flatpak_configuration_set_platform (self, g_value_get_string (value));
      break;

    case PROP_PRIMARY_MODULE:
      gbp_flatpak_configuration_set_primary_module (self, g_value_get_string (value));
      break;

    case PROP_SDK:
      gbp_flatpak_configuration_set_sdk (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_flatpak_configuration_finalize (GObject *object)
{
  GbpFlatpakConfiguration *self = (GbpFlatpakConfiguration *)object;

  g_clear_pointer (&self->branch, g_free);
  g_clear_pointer (&self->command, g_free);
  g_clear_pointer (&self->finish_args, g_strfreev);
  g_clear_object (&self->manifest);
  g_clear_pointer (&self->platform, g_free);
  g_clear_pointer (&self->primary_module, g_free);
  g_clear_pointer (&self->sdk, g_free);

  G_OBJECT_CLASS (gbp_flatpak_configuration_parent_class)->finalize (object);
}

static void
gbp_flatpak_configuration_class_init (GbpFlatpakConfigurationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeConfigurationClass *config_class = IDE_CONFIGURATION_CLASS (klass);

  object_class->finalize = gbp_flatpak_configuration_finalize;
  object_class->get_property = gbp_flatpak_configuration_get_property;
  object_class->set_property = gbp_flatpak_configuration_set_property;

  config_class->supports_runtime = gbp_flatpak_configuration_supports_runtime;

  properties [PROP_BRANCH] =
    g_param_spec_string ("branch",
                         "Branch",
                         "Branch",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_COMMAND] =
    g_param_spec_string ("command",
                         "Command",
                         "Command",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_FINISH_ARGS] =
    g_param_spec_boxed ("finish-args",
                        "Finish args",
                        "Finish args",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  properties [PROP_MANIFEST] =
    g_param_spec_object ("manifest",
                         "Manifest",
                         "Manifest file",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_PLATFORM] =
    g_param_spec_string ("platform",
                         "Platform",
                         "Platform",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIMARY_MODULE] =
    g_param_spec_string ("primary-module",
                         "Primary module",
                         "Primary module",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_SDK] =
    g_param_spec_string ("sdk",
                         "Sdk",
                         "Sdk",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_flatpak_configuration_init (GbpFlatpakConfiguration *self)
{
  ide_configuration_set_prefix (IDE_CONFIGURATION (self), "/app");
}
