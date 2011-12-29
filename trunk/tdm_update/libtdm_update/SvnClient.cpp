/*****************************************************************************
                    The Dark Mod GPL Source Code
 
 This file is part of the The Dark Mod Source Code, originally based 
 on the Doom 3 GPL Source Code as published in 2011.
 
 The Dark Mod Source Code is free software: you can redistribute it 
 and/or modify it under the terms of the GNU General Public License as 
 published by the Free Software Foundation, either version 3 of the License, 
 or (at your option) any later version. For details, see LICENSE.TXT.
 
 Project: The Dark Mod Updater (http://www.thedarkmod.com/)
 
 $Revision$ (Revision of last commit) 
 $Date$ (Date of last commit)
 $Author$ (Author of last commit)
 
******************************************************************************/

#include "SvnClient.h"

#include "SvnClientImpl.h"

namespace tdm
{

// Factory method to retrieve an implementation of this class
SvnClientPtr SvnClient::Create()
{
#ifdef USE_LIBSVN
	// libsvn enabled, return implementation using actual svn calls
	return SvnClientPtr(new SvnClientImpl);
#else
	// No libsvn included, use dummy object
	return SvnClientPtr(new SvnClientDummy);
#endif
}

} // namespace
