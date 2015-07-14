/**
* ZNC Get Op
*
* Allows the user to redirect what boring ppl say to another (fake) chan.
*
* Copyright (c) 2012 Romain Labolle
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License version 2 as published
* by the Free Software Foundation.
*/

#include <znc/znc.h>
#include <znc/Chan.h>
#include <znc/User.h>
#include <znc/Modules.h>
#include <znc/IRCNetwork.h>

using std::map;
using std::vector;

class CGetOpMod : public CModule {
public:
	MODCONSTRUCTOR(CGetOpMod) {}

	virtual bool OnLoad(const CString& sArgs, CString& sErrorMsg) {
		return true;
	}

	virtual ~CGetOpMod() {}

	virtual void update(CChan& Channel) {
		const map<CString,CNick>& Nicks = Channel.GetNicks();

		for (map<CString,CNick>::const_iterator it = Nicks.begin(); it != Nicks.end(); ++it) {
			if (it->second.HasPerm('@'))
				return;
		}
		PutIRC("PRIVMSG R :REQUESTOP " + Channel.GetName() + " " + GetNetwork()->GetIRCNick().GetNick());
		return;
	}

	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
		update(Channel);
		return;
	}

	virtual void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) {
		update(Channel);
		return;
	}

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {
		for(vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it)
		{
			update(**it);
		}
		return;
	}
	
	virtual EModRet OnRaw(CString& sLine) {
		// :irc.server.com 366 nick #chan :End of /NAMES list.
		if (sLine.Token(1) == "366")
		{
			CChan* chan = GetNetwork()->FindChan(sLine.Token(3));
			if(chan)
				update(*chan);
		}
		return CONTINUE;
	}
private:
};

NETWORKMODULEDEFS(CGetOpMod, "Get op")
