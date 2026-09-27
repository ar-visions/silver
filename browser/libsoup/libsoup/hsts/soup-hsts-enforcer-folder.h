/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2026 Kalen White
 */

#pragma once

#include "soup-hsts-enforcer.h"

G_BEGIN_DECLS

#define SOUP_TYPE_HSTS_ENFORCER_FOLDER (soup_hsts_enforcer_folder_get_type ())
SOUP_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (SoupHSTSEnforcerFolder, soup_hsts_enforcer_folder, SOUP, HSTS_ENFORCER_FOLDER, SoupHSTSEnforcer)

SOUP_AVAILABLE_IN_ALL
SoupHSTSEnforcer *soup_hsts_enforcer_folder_new (const char *directory);

G_END_DECLS
