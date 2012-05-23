/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * Contact: mfs-dm-2011@igalia.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */
#include <config.h>
#include <glib/gi18n.h>

#include "gt-feed-server.h"
#include "gt-feed-server-private.h"

#include "gt-feed.h"

G_DEFINE_TYPE(GtFeedServer, gt_feed_server, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
                (G_TYPE_INSTANCE_GET_PRIVATE((o), GT_TYPE_FEED_SERVER, GtFeedServerPrivate))

enum {
        PROP_APIKEY = 1,
};

struct _GtFeedServerPrivate {
        GtFeed *feed;
        GDBusNodeInfo *dbusinfo;
};

static void
get_property(GObject *object, guint property_id,
             GValue *value, GParamSpec *pspec)
{
        GtFeedServer *self = GT_FEED_SERVER(object);
        GObject *feed = G_OBJECT(self->priv->feed);

        switch (property_id) {
                case PROP_APIKEY:
                        g_object_get_property(feed, "api-key", value);
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        }
}

static void
set_property(GObject *object, guint property_id,
             const GValue *value, GParamSpec *pspec)
{
        GtFeedServer *self = GT_FEED_SERVER(object);
        GObject *feed = G_OBJECT(self->priv->feed);

        switch (property_id) {
                case PROP_APIKEY:
                        g_object_set_property(feed, "api-key", value);
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        }
}

static void
dispose(GObject *object)
{
        GtFeedServer *self = GT_FEED_SERVER(object);
        g_object_unref(self->priv->feed);

        G_OBJECT_CLASS(gt_feed_server_parent_class)->dispose(object);
}

static void
finalize(GObject *object)
{
        GtFeedServer *self = GT_FEED_SERVER(object);
        g_dbus_node_info_unref(self->priv->dbusinfo);

        G_OBJECT_CLASS(gt_feed_server_parent_class)->finalize(object);
}

static void
gt_feed_server_class_init(GtFeedServerClass *klass)
{
        GObjectClass *gclass = G_OBJECT_CLASS(klass);

        g_type_class_add_private(klass, sizeof(GtFeedServerPrivate));

        gclass->dispose = dispose;
        gclass->finalize = finalize;
        gclass->set_property = set_property;
        gclass->get_property = get_property;

        g_object_class_install_property
                (gclass, PROP_APIKEY,
                 g_param_spec_string("api-key", "API key", "Trakt.tv API key",
                                     NULL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT |
                                     G_PARAM_STATIC_STRINGS));
}

static void
response_cb(GtFeed *feed, gpointer data)
{
        GError *error = NULL;
        GDBusConnection *connection =
                g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

        if (!connection) {
                g_printerr(_("Error getting session bus: %s\n"), error->message);
                g_error_free(error);
                return;
        }

        g_dbus_connection_emit_signal(connection,
                                      NULL,
                                      "/org/mfs/Gtrakt/FeedServer",
                                      "org.mfs.Gtrakt.FeedServer",
                                      "ResponseReceived",
                                      NULL,
                                      &error);

        if (error) {
                g_printerr(_("Error emitting signal: %s\n"), error->message);
                g_error_free(error);
        }

        g_object_unref(connection);
}

static void
gt_feed_server_init(GtFeedServer *self)
{
        GError *error = NULL;
        GtFeedServerPrivate *priv;

        priv = self->priv = GET_PRIVATE(self);

        priv->feed = g_object_new(GT_TYPE_FEED, NULL);
        g_signal_connect(self->priv->feed, "response-received",
                         G_CALLBACK(response_cb), NULL);

        priv->dbusinfo = g_dbus_node_info_new_for_xml(interface_xml, &error);
        if (error) {
                g_critical(_("Couldn't parse gdbus node info: %s"),
                           error->message);
                g_error_free(error);
        }
}

GtFeedServer*
gt_feed_server_new(const gchar *apikey)
{
        return g_object_new(GT_TYPE_FEED_SERVER, "api-key", apikey);
}

static GVariant *
deal_maybe(GVariant *item)
{
        GVariant *child;

        if (!g_variant_is_of_type(item, G_VARIANT_TYPE_MAYBE))
                return item;

        child = g_variant_get_maybe(item);
        if (!child)
                return g_variant_new_string("");

        return child;
}


static GVariant *
filter_item(GVariant *item)
{
        GVariantBuilder *builder;
        GVariantIter iter;
        GVariant *child;

        if (!g_variant_is_container(item)) {
                return deal_maybe(item);
        }

        builder = g_variant_builder_new(g_variant_get_type(item));
        g_variant_iter_init(&iter, item);

        while ((child = g_variant_iter_next_value(&iter))) {
                g_variant_builder_add_value(builder,
                                            filter_item(deal_maybe(child)));
                g_variant_unref(child);
        }

        return g_variant_builder_end(builder);
}

static GVariant *
filter_search_result(GVariant *content)
{
        if (g_variant_n_children(content) == 0)
                return g_variant_ref(content);

        return filter_item(content);
}

static void
cb(GObject *source,
   GAsyncResult *res,
   void *data)
{
        GVariant *content, *filtered;
        GError *error = NULL;
        GtFeed *feed = GT_FEED(source);
        GDBusMethodInvocation *invocation = data;

        content = gt_feed_search_finish(feed, res, &error);
        if (error) {
                g_dbus_method_invocation_take_error(invocation, error);
                return;
        }

        filtered = filter_search_result(content);
        g_variant_unref(content);
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new_tuple(&filtered,1));
}

static void
query(GtFeedServer *self,
      GVariant *parameters,
      GDBusMethodInvocation *invocation)
{
        gchar *query;
        gint type;
        gboolean res;
        GtFeed *feed = self->priv->feed;

        query = NULL;
        g_variant_get(parameters, "(si)", &query, &type);

        if (!query) {
                g_dbus_method_invocation_return_error
                        (invocation,
                         GT_FEED_SERVER_ERROR,
                         GT_FEED_SERVICE_ERROR_MISSING_PARAMETER,
                         N_("No query was specified."));
                return;
        }

        if (type < GT_FEED_SEARCH_MOVIES || type >= GT_FEED_SEARCH_MAX) {
                g_dbus_method_invocation_return_error
                        (invocation,
                         GT_FEED_SERVER_ERROR,
                         GT_FEED_SERVICE_ERROR_MISSING_PARAMETER,
                         N_("Search type is not valid."));
                return;
        }

        res = gt_feed_search(feed, type, query, cb, invocation);
        g_free(query);

        if (!res) {
                g_dbus_method_invocation_return_error
                        (invocation,
                         GT_FEED_SERVER_ERROR,
                         GT_FEED_SERVICE_ERROR_INVALID_QUERY,
                         N_("The specified query is invalid."));
        }
}

static void
method_call(GDBusConnection *connection,
            const gchar *sender,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *method_name,
            GVariant *parameters,
            GDBusMethodInvocation *invocation,
            gpointer data)
{
        GtFeedServer *self = GT_FEED_SERVER(data);

        if (g_strcmp0(method_name, "Query") == 0) {
                query(self, parameters, invocation);
        } else {
                g_object_unref(invocation);
        }
}

static GVariant *
get_property_call(GDBusConnection *connection,
                  const gchar *sender,
                  const gchar *object_path,
                  const gchar *interface_name,
                  const gchar *property_name,
                  GError **error,
                  gpointer data)
{
        GtFeedServer *self = GT_FEED_SERVER(data);
        GVariant *ret = NULL;

        if (g_strcmp0(property_name, "ApiKey") == 0) {
                gchar *apikey = NULL;

                g_object_get(self, "api-key", &apikey, NULL);
                ret = g_variant_new("s", apikey ? apikey : "");
                g_free(apikey);
        }

        return ret;
}

static gboolean
apikey_set(GtFeedServer *self,
           GDBusConnection *connection,
           const gchar *object_path,
           GVariant *value)
{
        GError *error = NULL;
        const gchar *apikey = g_variant_get_string(value, NULL);

        if (!apikey)
                return TRUE;

        g_object_set(self, "api-key", apikey, NULL);

        g_dbus_connection_emit_signal(connection,
                                      NULL,
                                      object_path,
                                      "org.freedesktop.DBus.Properties",
                                      "PropertiesChanged",
                                      NULL,
                                      &error);
        if (error) {
                g_error_free(error);
                return FALSE;
        }

        return TRUE;
}

static gboolean
set_property_call(GDBusConnection *connection,
                  const gchar *sender,
                  const gchar *object_path,
                  const gchar *interface_name,
                  const gchar *property_name,
                  GVariant *value,
                  GError **error,
                  gpointer data)
{
        GtFeedServer *self = GT_FEED_SERVER(data);
        gboolean ret = TRUE;

        if (g_strcmp0(property_name, "ApiKey") == 0) {
                ret = apikey_set(self, connection, object_path, value);
        }

        return ret;
}

static const GDBusInterfaceVTable iface_vtable = {
        .method_call = method_call,
        .get_property = get_property_call,
        .set_property = set_property_call,
};

guint
gt_feed_server_register(GtFeedServer *self,
                        GDBusConnection *connection,
                        const gchar *path,
                        GError **error)
{
        guint res;
        GDBusNodeInfo *dbusinfo;

        g_return_val_if_fail(self &&
                             self->priv &&
                             self->priv->dbusinfo, 0);

        dbusinfo = self->priv->dbusinfo;

        res = g_dbus_connection_register_object(connection,
                                                path,
                                                dbusinfo->interfaces[0],
                                                &iface_vtable,
                                                self,
                                                NULL,
                                                error);

        return res;
}

void
gt_feed_server_set_apikey(GtFeedServer *self, const gchar *apikey)
{
        g_return_if_fail(self && self->priv && self->priv->feed && apikey);

        g_object_set(self->priv->feed, "api-key", apikey, NULL);
}

static const GDBusErrorEntry dbus_errors[] = {
        {
                .error_code = GT_FEED_SERVICE_ERROR_MISSING_PARAMETER,
                .dbus_error_name = "org.mfs.GtraktError.MissingParameter",
        },
        {
                .error_code = GT_FEED_SERVICE_ERROR_INVALID_QUERY,
                .dbus_error_name = "org.mfs.GtraktError.InvalidQuery",
        },
};

GQuark
gt_feed_server_error_quark(void)
{
        static volatile gsize quark = 0;

        g_dbus_error_register_error_domain("feed-server-error-quark",
                                           &quark,
                                           dbus_errors,
                                           G_N_ELEMENTS(dbus_errors));

        return (GQuark) quark;
}
