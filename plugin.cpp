/***************************************************************************
 * Copyright (C) comoglu@gmail.com                                         *
 * All rights reserved.                                                    *
 *                                                                         *
 * GNU Affero General Public License Usage                                 *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 ***************************************************************************/


#include <seiscomp/core/plugin.h>


ADD_SC_PLUGIN(
	"XYZ/slippy-map tile store — supports OSM, OTM, ESRI, CartoDB and any XYZ tile server",
	"comoglu@gmail.com",
	1, 1, 0
)
