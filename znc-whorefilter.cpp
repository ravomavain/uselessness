/**
* ZNC Whore Filter
*
* Allows the user to redirect what boring ppl say to another (fake) chan.
*
* Copyright (c) 2012 Romain Labolle
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License version 2 as published
* by the Free Software Foundation.
*/

#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"


class CWhoreMod : public CModule {
public:
	MODCONSTRUCTOR(CWhoreMod) {}

	virtual bool OnLoad(const CString& sArgs, CString& sErrorMsg) {
		m_sHostmask = "attentionwhor*!*srs@*";
		m_sChannel = "#srsbsns";
		m_sNewChan = "~#whorefilter";
		return true;
	}

	virtual ~CWhoreMod() {}

	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
		if (Nick.GetHostMask().WildCmp(m_sHostmask) && Channel.GetName().AsLower().WildCmp(m_sChannel)) {
			PutUser(":" + Nick.GetHostMask() + " PRIVMSG " + m_sNewChan + " :" + sMessage);
			return HALT;
		}
		return CONTINUE;
	}
private:
	CString m_sHostmask;
	CString m_sChannel;
	CString m_sNewChan;
};

MODULEDEFS(CWhoreMod, "Filter redirect whore msg to another (fake) chan")
