#!/bin/bash

if [ -z "$MESON_BUILD_ROOT" ]; then
	exit 1
fi

cd "$MESON_BUILD_ROOT" || exit 1

# We have to deal with gschema ourselves...
tmpdir=$(mktemp -d -p "$MESON_BUILD_ROOT" -t 'schema-tmp.XXXXXXXXXX')
function cleanup_temp() { rm -rf "$tmpdir"; }
trap cleanup_temp EXIT
cp $MESON_SOURCE_ROOT/data/gsettings/*.gschema.xml "$tmpdir"
cp $MESON_BUILD_ROOT/data/gsettings/*.gschema.xml "$tmpdir"
cp $MESON_SOURCE_ROOT/plugins/gnome-code-assistance/*.gschema.xml "$tmpdir"
glib-compile-schemas "$tmpdir" || exit 1

# We have to deal with plugins ourselves...
pushd plugins
for plugin in *; do
	# FIXME: Copy Python plugins
	cp $MESON_SOURCE_ROOT/plugins/$plugin/*.plugin $MESON_BUILD_ROOT/plugins/$plugin
done
popd

env	PEAS_DEBUG=1 \
	GB_IN_TREE_PLUGINS=1 \
	GB_IN_TREE_FONTS=1 \
	GB_IN_TREE_STYLE_SCHEMES=1 \
	GI_TYPELIB_PATH="libide:contrib/egg:contrib/pnl:contrib/tmpl:$GI_TYPELIB_PATH" \
	GOBJECT_DEBUG=instance-count \
	GSETTINGS_SCHEMA_DIR="$tmpdir" \
	PYTHONDONTWRITEBYTECODE=yes \
	./gnome-builder -vvvvs

