/*
 * Dynamic Lua binding to GObject using dynamic gobject-introspection.
 *
 * Copyright (c) 2010 Pavel Holejsovsky
 * Licensed under the MIT license:
 * http://www.opensource.org/licenses/mit-license.php
 *
 * Native Lua wrappers around GIRepository.
 */

#include <string.h>
#include "lgi.h"

typedef GIBaseInfo *(* InfosItemGet)(GIBaseInfo* info, gint item);

/* Creates new instance of info from given GIBaseInfo pointer. */
int
lgi_gi_info_new (lua_State *L, GIBaseInfo *info)
{
  if (info)
    {
      GIBaseInfo **ud_info;

      if (g_base_info_get_type (info) == GI_INFO_TYPE_INVALID)
	{
	  g_base_info_unref (info);
	  lua_pushnil (L);
	}
      else
	{
	  ud_info = lua_newuserdata (L, sizeof (info));
	  *ud_info = info;
	  luaL_getmetatable (L, LGI_GI_INFO);
	  lua_setmetatable (L, -2);
	}
    }
  else
    lua_pushnil (L);

  return 1;
}

/* Userdata representing single group of infos (e.g. methods on
   object, fields of struct etc.).  Emulates Lua table for access. */
typedef struct _Infos
{
  GIBaseInfo *info;
  gint count;
  InfosItemGet item_get;
} Infos;
#define LGI_GI_INFOS "lgi.gi.infos"

static int
infos_len (lua_State *L)
{
  Infos* infos = luaL_checkudata (L, 1, LGI_GI_INFOS);
  lua_pushnumber (L, infos->count);
  return 1;
}

static int
infos_index (lua_State *L)
{
  Infos* infos = luaL_checkudata (L, 1, LGI_GI_INFOS);
  gint n;
  if (lua_isnumber (L, 2))
    {
      n = lua_tointeger (L, 2) - 1;
      luaL_argcheck (L, n >= 0 && n < infos->count, 2, "out of bounds");
      return lgi_gi_info_new (L, infos->item_get (infos->info, n));
    }
  else
    {
      const gchar *name = luaL_checkstring (L, 2);
      for (n = 0; n < infos->count; n++)
	{
	  GIBaseInfo *info = infos->item_get (infos->info, n);
	  if (strcmp (g_base_info_get_name (info), name) == 0)
	    return lgi_gi_info_new (L, info);

	  g_base_info_unref (info);
	}

      lua_concat (L, lgi_type_get_name (L, infos->info));
      return luaL_error (L, "%s: `%s' not found", lua_tostring (L, -1), name);
    }
}

static int
infos_gc (lua_State *L)
{
  GIBaseInfo **info = luaL_checkudata (L, 1, LGI_GI_INFOS);
  g_base_info_unref (*info);
  return 0;
}

/* Creates new userdata object representing given category of infos. */
static int
infos_new (lua_State *L, GIBaseInfo *info, gint count, InfosItemGet item_get)
{
  Infos *infos = lua_newuserdata (L, sizeof (Infos));
  luaL_getmetatable (L, LGI_GI_INFOS);
  lua_setmetatable (L, -2);
  infos->info = g_base_info_ref (info);
  infos->count = count;
  infos->item_get = item_get;
  return 1;
}

static const luaL_Reg gi_infos_reg[] = {
  { "__gc", infos_gc },
  { "__len", infos_len },
  { "__index", infos_index },
  { NULL, NULL }
};

static int
info_index (lua_State *L)
{
  GIBaseInfo **info = luaL_checkudata (L, 1, LGI_GI_INFO);
  const gchar *prop = luaL_checkstring (L, 2);

#define INFOS(n1, n2)							\
  else if (strcmp (prop, #n2 "s") == 0)					\
    return infos_new (L, *info,						\
		      g_ ## n1 ## _info_get_n_ ## n2 ## s (*info),	\
		      g_ ## n1 ## _info_get_ ## n2);

#define INFOS2(n1, n2, n3)					\
  else if (strcmp (prop, #n3) == 0)				\
    return infos_new (L, *info,					\
		      g_ ## n1 ## _info_get_n_ ## n3 (*info),	\
		      g_ ## n1 ## _info_get_ ## n2);

  if (strcmp (prop, "type") == 0)
    {
      switch (g_base_info_get_type (*info))
	{
#define H(n1, n2)				\
	  case GI_INFO_TYPE_ ## n1:		\
	    lua_pushstring (L, #n2);		\
	    return 1;

	  H(FUNCTION, function)
	    H(CALLBACK, callback)
	    H(STRUCT, struct)
	    H(BOXED, boxed)
	    H(ENUM, enum)
	    H(FLAGS, flags)
	    H(OBJECT, object)
	    H(INTERFACE, interface)
	    H(CONSTANT, constant)
	    H(ERROR_DOMAIN, error_domain)
	    H(UNION, union)
	    H(VALUE, value)
	    H(SIGNAL, signal)
	    H(VFUNC, vfunc)
	    H(PROPERTY, property)
	    H(FIELD, field)
	    H(ARG, arg)
	    H(TYPE, type)
	    H(UNRESOLVED, unresolved)
#undef H
	default:
	  g_assert_not_reached ();
	}
    }

#define H(n1, n2)						\
  if (strcmp (prop, "is_" #n2) == 0)				\
    {								\
      lua_pushboolean (L, GI_IS_ ## n1 ## _INFO (*info));	\
      return 1;							\
    }
  H(ARG, arg)
    H(CALLABLE, callable)
    H(FUNCTION, function)
    H(SIGNAL, signal)
    H(VFUNC, vfunc)
    H(CONSTANT, constant)
    H(ERROR_DOMAIN, error_domain)
    H(FIELD, field)
    H(PROPERTY, property)
    H(REGISTERED_TYPE, registered_type)
    H(ENUM, enum)
    H(INTERFACE, interface)
    H(OBJECT, object)
    H(STRUCT, struct)
    H(UNION, union)
    H(TYPE, type)
    H(VALUE, value);
#undef H

  if  (!GI_IS_TYPE_INFO (*info))
    {
      if (strcmp (prop, "name") == 0)
	{
	  lua_pushstring (L, g_base_info_get_name (*info));
	  return 1;
	}
      else if (strcmp (prop, "namespace") == 0)
	{
	  lua_pushstring (L, g_base_info_get_namespace (*info));
	  return 1;
	}
    }

  if (strcmp (prop, "fullname") == 0)
    {
      lua_concat (L, lgi_type_get_name (L, *info));
      return 1;
    }

  if (strcmp (prop, "deprecated") == 0)
    {
      lua_pushboolean (L, g_base_info_is_deprecated (*info));
      return 1;
    }
  else if (strcmp (prop, "container") == 0)
    {
      GIBaseInfo *container = g_base_info_get_container (*info);
      if (container)
	g_base_info_ref (container);
      return lgi_gi_info_new (L, container);
    }
  else if (strcmp (prop, "typeinfo") == 0)
    {
      GITypeInfo *ti = NULL;
      if (GI_IS_ARG_INFO (*info))
	ti = g_arg_info_get_type (*info);
      else if (GI_IS_CONSTANT_INFO (*info))
	ti = g_constant_info_get_type (*info);
      else if (GI_IS_PROPERTY_INFO (*info))
	ti = g_property_info_get_type (*info);
      else if (GI_IS_FIELD_INFO (*info))
	ti = g_field_info_get_type (*info);

      if (ti)
	return lgi_gi_info_new (L, ti);
    }

  if (GI_IS_REGISTERED_TYPE_INFO (*info))
    {
      if (strcmp (prop, "gtype") == 0)
	{
	  lua_pushnumber (L, g_registered_type_info_get_g_type (*info));
	  return 1;
	}
      else if (GI_IS_STRUCT_INFO (*info))
	{
	  if (strcmp (prop, "is_gtype_struct") == 0)
	    {
	      lua_pushboolean (L, g_struct_info_is_gtype_struct (*info));
	      return 1;
	    }
	  INFOS (struct, field)
	    INFOS (struct, method);
	}
      else if (GI_IS_UNION_INFO (*info))
	{
	  if (0);
	  INFOS (union, field)
	    INFOS (union, method);
	}
      else if (GI_IS_INTERFACE_INFO (*info))
	{
	  if (0);
	  INFOS (interface, prerequisite)
	    INFOS (interface, method)
	    INFOS (interface, constant)
	    INFOS2 (interface, property, properties)
	    INFOS (interface, signal);
	}
      else if (GI_IS_OBJECT_INFO (*info))
	{
	  if (strcmp (prop, "parent") == 0)
	    return lgi_gi_info_new (L, g_object_info_get_parent (*info));
	  INFOS (object, interface)
	    INFOS (object, field)
	    INFOS (object, method)
	    INFOS (object, constant)
	    INFOS2 (object, property, properties)
	    INFOS (object, signal);
	}
    }

  if (GI_IS_CALLABLE_INFO (*info))
    {
      if (strcmp (prop, "return_type") == 0)
	return lgi_gi_info_new (L, g_callable_info_get_return_type (*info));
      INFOS (callable, arg);

      if (GI_IS_SIGNAL_INFO (*info))
	{
	  if (strcmp (prop, "flags") == 0)
	    {
	      GSignalFlags flags = g_signal_info_get_flags (*info);
	      lua_newtable (L);
	      if (0);
#define H(n1, n2)					\
	      else if ((flags & G_SIGNAL_ ## n1) != 0)	\
		{					\
		  lua_pushboolean (L, 1);		\
		  lua_setfield (L, -2, #n2);		\
		}
	      H(RUN_FIRST, run_first)
		H(RUN_LAST, run_last)
		H(RUN_CLEANUP, run_cleanup)
		H(NO_RECURSE, no_recurse)
		H(DETAILED, detailed)
		H(ACTION, action)
		H(NO_HOOKS, no_hooks);
#undef H
	      return 1;
	    }
	}

      if (GI_IS_FUNCTION_INFO (*info))
	{
	  if (strcmp (prop, "flags") == 0)
	    {
	      GIFunctionInfoFlags flags = g_function_info_get_flags (*info);
	      lua_newtable (L);
	      if (0);
#define H(n1, n2)					\
	      else if ((flags & GI_FUNCTION_ ## n1) != 0)	\
		{					\
		  lua_pushboolean (L, 1);		\
		  lua_setfield (L, -2, #n2);		\
		}
	      H(IS_METHOD, is_method)
		H(IS_CONSTRUCTOR, is_constructor)
		H(IS_GETTER, is_getter)
		H(IS_SETTER, is_setter)
		H(WRAPS_VFUNC, wraps_vfunc)
		H(THROWS, throws);
#undef H
	      return 1;
	    }
	}
    }

  if (GI_IS_ENUM_INFO (*info))
    {
      if (strcmp (prop, "storage") == 0)
	{
	  GITypeTag tag = g_enum_info_get_storage_type (*info);
	  lua_pushstring (L, g_type_tag_to_string (tag));
	  return 1;
	}
      INFOS (enum, value);
    }

  if (GI_IS_VALUE_INFO (*info))
    {
      if (strcmp (prop, "value") == 0)
	{
	  lua_pushnumber (L, g_value_info_get_value (*info));
	  return 1;
	}
    }

  if (GI_IS_TYPE_INFO (*info))
    {
      GITypeTag tag = g_type_info_get_tag (*info);
      if (strcmp (prop, "tag") == 0)
	{
	  lua_pushstring (L, g_type_tag_to_string (tag));
	  return 1;
	}
      else if (strcmp (prop, "is_basic") == 0)
	{
	  lua_pushboolean (L, G_TYPE_TAG_IS_BASIC (tag));
	  return 1;
	}
      else if (strcmp (prop, "params") == 0)
	{
	  if (tag == GI_TYPE_TAG_ARRAY || tag == GI_TYPE_TAG_GLIST ||
	      tag == GI_TYPE_TAG_GSLIST || tag == GI_TYPE_TAG_GHASH)
	    {
	      lua_newtable (L);
	      lgi_gi_info_new (L, g_type_info_get_param_type (*info, 0));
	      lua_rawseti (L, -2, 1);
	      if (tag == GI_TYPE_TAG_GHASH)
		{
		  lgi_gi_info_new (L, g_type_info_get_param_type (*info, 1));
		  lua_rawseti (L, -2, 2);
		}
	      return 1;
	    }
	}
      else if (strcmp (prop, "interface") == 0 && tag == GI_TYPE_TAG_INTERFACE)
	{
	  lgi_gi_info_new (L, g_type_info_get_interface (*info));
	  return 1;
	}
      else if (strcmp (prop, "array_type") == 0 && tag == GI_TYPE_TAG_ARRAY)
	{
	  switch (g_type_info_get_array_type (*info))
	    {
#define H(n1, n2)				\
	      case GI_ARRAY_TYPE_ ## n1:	\
		lua_pushstring (L, #n2);	\
		return 1;

	      H(C, c)
		H(ARRAY, array)
		H(PTR_ARRAY, ptr_array)
		H(BYTE_ARRAY, byte_array)
#undef H
	    default:
	      g_assert_not_reached ();
	    }
	}
    }

  lua_pushnil (L);
  return 1;

#undef INFOS
#undef INFOS2
}

static int
info_gc (lua_State *L)
{
  GIBaseInfo **info = luaL_checkudata (L, 1, LGI_GI_INFO);
  g_base_info_unref (*info);
  return 0;
}

static const luaL_Reg gi_info_reg[] = {
  { "__gc", info_gc },
  { "__index", info_index },
  { NULL, NULL }
};

/* Userdata representing namespace in girepository. */
#define LGI_GI_NAMESPACE "lgi.gi.namespace"

static int
namespace_len (lua_State *L)
{
  const gchar *ns = luaL_checkudata (L, 1, LGI_GI_NAMESPACE);
  lua_pushinteger (L, g_irepository_get_n_infos (NULL, ns) + 1);
  return 1;
}

static int
namespace_index (lua_State *L)
{
  const gchar *ns = luaL_checkudata (L, 1, LGI_GI_NAMESPACE);
  const gchar *prop;
  if (lua_isnumber (L, 2))
    {
      GIBaseInfo *info = g_irepository_get_info (NULL, ns,
						 lua_tointeger (L, 2) - 1);
      return lgi_gi_info_new (L, info);
    }
  prop = luaL_checkstring (L, 2);
  if (strcmp (prop, "dependencies") == 0)
    {
      gchar **deps = g_irepository_get_dependencies (NULL, ns);
      if (deps == NULL)
	lua_pushnil (L);
      else
	{
	  int index;
	  gchar **dep;
	  lua_newtable (L);
	  for (index = 1, dep = deps; *dep; dep++, index++)
	    {
	      const gchar *sep = strchr (*dep, '-');
	      lua_pushlstring (L, *dep, sep - *dep);
	      lua_pushstring (L, sep + 1);
	      lua_settable (L, -3);
	    }
	  g_strfreev (deps);
	}

      return 1;
    }
  else if (strcmp (prop, "version") == 0)
    {
      lua_pushstring (L, g_irepository_get_version (NULL, ns));
      return 1;
    }
  else
    /* Try to lookup the symbol. */
    return lgi_gi_info_new (L, g_irepository_find_by_name (NULL, ns, prop));
}

static int
namespace_new (lua_State *L, const gchar *namespace)
{
  gchar *ns = lua_newuserdata (L, strlen (namespace) + 1);
  luaL_getmetatable (L, LGI_GI_NAMESPACE);
  lua_setmetatable (L, -2);
  strcpy (ns, namespace);
  return 1;
}

static const luaL_Reg gi_namespace_reg[] = {
  { "__index", namespace_index },
  { "__len", namespace_len },
  { NULL, NULL }
};

/* Lua API: core.gi.require(namespace[, version[, typelib_dir]]) */
static int
gi_require (lua_State *L)
{
  GError *err = NULL;
  const gchar *namespace = luaL_checkstring (L, 1);
  const gchar *version = luaL_optstring (L, 2, NULL);
  const gchar *typelib_dir = luaL_optstring (L, 3, NULL);
  GITypelib *typelib;

  if (typelib_dir == NULL)
    typelib = g_irepository_require (NULL, namespace, version,
				     G_IREPOSITORY_LOAD_FLAG_LAZY, &err);
  else
    typelib = g_irepository_require_private (NULL, typelib_dir, namespace,
					     version,
					     G_IREPOSITORY_LOAD_FLAG_LAZY,
					     &err);
  if (!typelib)
    {
      lua_pushboolean (L, 0);
      lua_pushstring (L, err->message);
      lua_pushnumber (L, err->code);
      g_error_free (err);
      return 3;
    }

  return namespace_new (L, namespace);
}

static int
gi_index (lua_State *L)
{
  if (lua_isnumber (L, 2))
    {
      GIBaseInfo *info =
	g_irepository_find_by_gtype (NULL, luaL_checknumber (L, 2));
      return lgi_gi_info_new (L, info);
    }
  else
    {
      const gchar *ns = luaL_checkstring (L, 2);
      if (g_irepository_is_registered (NULL, ns, NULL))
	return namespace_new (L, ns);
    }

  lua_pushnil (L);
  return 0;
}

typedef struct _Reg
{
  const gchar *name;
  const luaL_Reg* reg;
} Reg;

static const Reg gi_reg[] = {
  { LGI_GI_INFOS, gi_infos_reg },
  { LGI_GI_INFO, gi_info_reg },
  { LGI_GI_NAMESPACE, gi_namespace_reg },
  { NULL, NULL }
};

void
lgi_gi_init (lua_State *L)
{
  const Reg *reg;

  /* Register metatables for userdata objects. */
  for (reg = gi_reg; reg->name; reg++)
    {
      luaL_newmetatable (L, reg->name);
      luaL_register (L, NULL, reg->reg);
      lua_pop (L, 1);
    }

  /* Register global API. */
  lua_newtable (L);
  lua_pushcfunction (L, gi_require);
  lua_setfield (L, -2, "require");
  lua_newtable (L);
  lua_pushcfunction (L, gi_index);
  lua_setfield (L, -2, "__index");
  lua_setmetatable (L, -2);
  lua_setfield (L, -2, "gi");
}
