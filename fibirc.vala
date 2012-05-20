/*
 * Compile with :
 * valac fibirc.vala --pkg gee-1.0 --pkg gio-2.0
 */
class Decimal {
    private const int size = 1000000000;
    public Gee.ArrayList<int> integer = new Gee.ArrayList<int> ();
    
    public Decimal(int num)
    {
        this.integer.add(num%this.size);
        this.integer.add((num-(num%this.size))/this.size);
    }

    public Decimal add(Decimal dec)
    {
        int over=0;
        int maxi = int.max(this.integer.size, dec.integer.size);
        for(int i=0;i<maxi;i++)
        {
            int result;
            result = over;
            if(i < this.integer.size)
                result += this.integer[i];
            if(i < dec.integer.size)
                result += dec.integer[i];
            over = (result - (result%this.size))/this.size;
            result %= this.size;
            if(i < this.integer.size)
                this.integer.set(i,result);
            else
                this.integer.add(result);
        }
        if(over>0)
        {
            this.integer.add(over);
        }
        return this;
    }

    public string to_string()
    {
        bool first=true;
        var builder = new StringBuilder ();
        for(int i=this.integer.size-1;i>=0;i--)
        {
            if(first && this.integer[i]!=0) {
                builder.append(this.integer[i].to_string());
                first=false;
            } else if(!first) {
                long diff = (size-1).to_string().length - this.integer[i].to_string().length;
                if(diff>0)
                    for(;diff>0;diff--)
                        builder.append("0");
                builder.append(this.integer[i].to_string());
            }
        }
        if(builder.len==0)
            builder.append("0");
        return builder.str;
    }
}

class IRCClient {
    private MainLoop loop;
    private MainLoop recvloop;
    private SocketConnection conn;
    private unowned Thread<void*> thread;
    public string host {get; set;}
    public string channel {get; set;}
    public string key {get; set;}
    public uint16 port {get; set;}
    public string nickname {get; set;}

    public IRCClient () {
        this.loop = new MainLoop (null, false);
    }

    public void start () throws Error {
            this.init.begin();
            this.loop.run();
        try {
            thread = Thread.create<void*>( thread_recv, true );
        } catch(ThreadError e) {
            stderr.printf ("ERROR1%s\n", e.message);
        }
    }

    public void stop () {
        thread.join ();
    }

    private void* thread_recv() {
        recvloop = new MainLoop();
        recv.begin();
        recvloop.run();
	    return null;
    }

    public async void init () throws Error {
        try {
            var resolver = Resolver.get_default ();
            var addresses = yield resolver.lookup_by_name_async (this.host, null);
            var address = addresses.nth_data (0);
            print ("(debug) resolved %s to %s\n", this.host, address.to_string ());

            var socket_address = new InetSocketAddress (address, this.port);
            var client = new SocketClient ();
            this.conn = yield client.connect_async (socket_address, null);
            print ("(debug) connected to %s:%d\n", this.host, this.port);

        } catch (Error e) {
            stderr.printf ("%s\n", e.message);
        }
        this.loop.quit();

    }

    private async void recv () throws Error {
        try {
            var input = new DataInputStream (this.conn.input_stream);
            while(true) {
                var answer = yield input.read_line_async (1, null, null);
                if(answer != null)
                {
                    print ("(recv) %s\n", answer);
                    if(answer.has_prefix("ERROR")) {
                        print("We got an error!\n");
                        this.recvloop.quit();
                        break;
                    } else if(answer.has_prefix("PING")) {
                        this.write("%s\r\n".printf(answer.replace("PING","PONG")));
                    } else if(Regex.match_simple(":[^ ]* 376 .*",answer)) {
                        this.write("JOIN %s %s\r\n".printf(this.channel, this.key));
                    } else if(Regex.match_simple(":[^ ]* PRIVMSG [^ ]* :.*$",answer)) {
                        var priv = Regex.split_simple(":([^! ]*)![^ ]* PRIVMSG ([^ ]*) :(.*)",answer);
                        print("(%s => %s) %s\n",priv[1],priv[2],priv[3]);
                        if(Regex.match_simple("^\\!fib [0-9]*$",priv[3]))
                        {
                            int fibnum = int.parse(Regex.split_simple("\\!fib ([0-9]*)",priv[3])[1]);
                            this.write("PRIVMSG %s :Calculating the %dth Fibonacci number...\r\n".printf(this.channel, fibnum));
                            Decimal fibo = Fibonacci(fibnum);
                            string fib = fibo.to_string();
                            //this.write("PRIVMSG %s :The %dth Fibonacci number have %d digits and it's md5 sum is %s :p\r\n".printf(this.channel, int.parse(fibnum), fib.length, md5sum(fib)));
                            int msg_length = 400;
                            while(fib.length >= msg_length)
                            {
                                this.write("PRIVMSG %s :%s...\r\n".printf(this.channel, fib.slice(0,msg_length)));
                                fib = fib.splice(0,msg_length);
                            }
                            this.write("PRIVMSG %s :%s\r\n".printf(this.channel, fib));
                            this.write("PRIVMSG %s :Done.\r\n".printf(this.channel));
                        }
                    }
                }
            }
        } catch (Error e) {
            stderr.printf ("%s\n", e.message);
        }
    }

    public void write (string request) {
        try {
            this.conn.output_stream.write (request.data);
            print ("(send) %s",request);
        } catch (Error e) {
            stderr.printf ("%s\n", e.message);
        }
    }
/*
    string md5sum (string str) {
        Checksum md5 = new Checksum(ChecksumType.MD5);
        md5.update(str.data, str.length);
        return md5.get_string();
    }
*/
    static Decimal Fibonacci(int n, Decimal f_n_1 = new Decimal(1), Decimal f_n = new Decimal(0))
    {
        if (n == 0) 
            return f_n;
        else
            return Fibonacci(n-1, f_n, f_n_1.add(f_n));
    }
}

const OptionEntry[] option_entries = {
    { "host",       'h', 0, OptionArg.STRING, ref host,     "IRC server hostname",     null },
    { "port",       'p', 0, OptionArg.INT,    ref port,     "IRC server port",         null },
    { "channel",    'c', 0, OptionArg.STRING, ref channel,  "IRC server channel",      null },
    { "key",        'k', 0, OptionArg.STRING, ref channel,  "IRC server channel key",  null },
    { "nickname",   'n', 0, OptionArg.STRING, ref nickname, "IRC nickname",            null },
    { "username",   'u', 0, OptionArg.STRING, ref username, "IRC username",            null },
    { "password",   'p', 0, OptionArg.STRING, ref password, "IRC password",            null },
    { "realname",   'r', 0, OptionArg.STRING, ref realname, "IRC real name",           null },
    { null }
};

static string host;
static uint16 port;
static string channel;
static string key;
static string nickname;
static string username;
static string password;
static string realname;

static int main (string[] args) {
    host="irc.quakenet.org";
    port=6667;
    channel="#fibonacci";
    key="";
    nickname="fib%d".printf(Random.int_range (1, 99));
    username="fib";
    realname="Fibonacci bot";
    int delay=1;
    try {
        var opt_context = new OptionContext("- IRC bot");
        opt_context.set_help_enabled(true);
        opt_context.add_main_entries(option_entries, "ircbot");
        opt_context.parse(ref args);
    } catch (OptionError e) {
        stderr.printf("Option parsing failed: %s\n", e.message);
        return -1;
    }
    if(host!="")
    {
        try {
            while(true)
            {
                var irc = new IRCClient ();
                irc.host = host;
                irc.channel = channel;
                irc.key = key;
                irc.nickname = nickname;
                irc.port = port;
                irc.start ();
                if(password!=null) {
                    string passreq = "PASS %s\r\n".printf(password);
                    irc.write (passreq);
                }
                string nickreq = "NICK %s\r\n".printf(nickname);
                irc.write (nickreq);
                string userreq = "USER %s 8 * :%s\r\n".printf(username,realname);
                irc.write (userreq);
                irc.stop();
                print("Waiting %ds before reconnecting",delay);
                Thread.usleep(1000000*delay);
                delay<<=1;
            }
        } catch (Error e) {
            stderr.printf ("%s\n", e.message);
            return -1;
        }
    }
    return 0;
}
