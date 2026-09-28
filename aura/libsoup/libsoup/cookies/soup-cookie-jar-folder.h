/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2026 Kalen White
 */

#pragma once

#include "soup-cookie-jar.h"

G_BEGIN_DECLS

#define SOUP_TYPE_COOKIE_JAR_FOLDER (soup_cookie_jar_folder_get_type ())
SOUP_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (SoupCookieJarFolder, soup_cookie_jar_folder, SOUP, COOKIE_JAR_FOLDER, SoupCookieJar)

SOUP_AVAILABLE_IN_ALL
SoupCookieJar *soup_cookie_jar_folder_new (const char *directory,
					   gboolean    read_only);

G_END_DECLS
