/**
* ZNC dice bot
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

class CDiceMod : public CModule {
public:
	MODCONSTRUCTOR(CDiceMod) {}

	virtual bool OnLoad(const CString& sArgs, CString& sErrorMsg) {
		user = GetUser();
		HighScore = sArgs.Token(0).ToInt();
		PutModule("HighScore: "+CString(HighScore));
		lastturn = false;
		return true;
	}

	virtual ~CDiceMod() {}

	virtual void OnModCommand(const CString& sCommand) {
		if (sCommand.Token(0).Equals("high")) {
			HighScore = sCommand.Token(1).ToInt();
			PutModule("HighScore: "+CString(HighScore));
		}
	}

	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
		if (sMessage.Equals("!dice start")) {
			PutIRC("PRIVMSG " + Channel.GetName() + " :!dice join");
			return CONTINUE;
		}
		CNick nick = user->GetNick();
		if (Nick.GetNick().Equals("Nimda3"))
		{
			if (sMessage.Token(0).Equals(nick.GetNick()+"\'s") && sMessage.Token(1).Equals("turn."))
			{
				PutIRC("PRIVMSG " + Channel.GetName() + " :!dice roll");
				lastturn = false;
				return CONTINUE;
			}
			if (sMessage.Token(0).Equals(nick.GetNick()) && sMessage.Token(1).Equals("rolls"))
			{
//				ravomavain rolls a 2. Points: 49 + 2 => 51 - roll again or stand?
				int value = sMessage.Token(3).ToInt();
				if (value == 6) {
					return CONTINUE;
				}
				int saved = sMessage.Token(5).ToInt();
				int temp = sMessage.Token(7).ToInt();
				int total = saved + temp;
				if (lastturn) {
					if (total < score) {
						PutIRC("PRIVMSG " + Channel.GetName() + " :!dice roll");
						return CONTINUE;
					}
					PutIRC("PRIVMSG " + Channel.GetName() + " :!dice stand");
					return CONTINUE;
				}
				if (total == 49 || total >= HighScore) {
					PutIRC("PRIVMSG " + Channel.GetName() + " :!dice stand");
					return CONTINUE;
				}
				if (total < 49)
				{
					if (temp >= 20)
					{
						PutIRC("PRIVMSG " + Channel.GetName() + " :!dice stand");
						return CONTINUE;
					}
				}
				PutIRC("PRIVMSG " + Channel.GetName() + " :!dice roll");
				return CONTINUE;
			}
			if (sMessage.Left(37).Equals("You broke the highest sum record with"))
			{
				HighScore = sMessage.Token(7).ToInt()+1;
				PutModule("HighScore: "+CString(HighScore));
				return CONTINUE;
			}
			if (sMessage.Left(26).Equals("Dice game has been started"))
			{
				lastturn = false;
				return CONTINUE;
			}
			if (sMessage.Left(39).Equals("It is a really bad idea to \x02stand\x02 now."))
			{
				PutIRC("PRIVMSG " + Channel.GetName() + " :!dice roll");
				return CONTINUE;
			}
			if (sMessage.WildCmp("*get one more chance to beat your score*"))
			{
				lastturn = true;
				score = sMessage.Token(11).ToInt();
				return CONTINUE;
			}
		}
		return CONTINUE;
	}
private:
	CUser *user;
	int HighScore;
	bool lastturn;
	int score;
};

MODULEDEFS(CDiceMod, "Dice bot")
