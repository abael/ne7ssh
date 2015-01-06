/* An example of ne7ssh library usage. Please change the values in connectWitKeys
   function before compiling.

   This will work with openssh server if default shell of authenticating user is bash.
   When using a different shell or custom prompt replace " $" string in waitFor()
   method with a string corresponding with your shell prompt.

   If you are testing this with later openssh versions, make sure to add this
   option to your server's configuration file to enable password authentication:

   PasswordAuthentication yes
*/

#include <ne7ssh.h>
#include <iostream>

void reportError(const std::string &tag, Ne7sshError* errors)
{
    std::string errmsg;
    do
    {
        errmsg = errors->pop();
        if (errmsg.size() > 0)
        {
            std::cerr << tag << " failed with last error: " << errmsg << std::endl;
        }
    } while (errmsg.size() > 0);
}

int main(int argc, char* argv[])
{
    int channel1;
    const char* result;

    if (argc != 4)
    {
        std::cerr << "Error: Three arguments required: " << argv[0] << " <hostname> <username> <privatekeyfilename>" << std::endl;
        return EXIT_FAILURE;
    }
    std::shared_ptr<ne7ssh> ssh = ne7ssh::ne7sshCreate();
    // Set SSH connection options.
    ssh->setOptions("aes128-cbc", "hmac-sha1");

    // Initiate connection.
    channel1 = ssh->connectWithKey(argv[1], 22, argv[2], argv[3]);
    if (channel1 < 0)
    {
        reportError("Connection", ssh->errors());
        return EXIT_FAILURE;
    }

    // Wait for bash prompt, or die in 5 seconds.
    if (!ssh->waitFor(channel1, "$", 5))
    {
        reportError("Wait for prompt", ssh->errors());
        ssh->close(channel1);
        return EXIT_FAILURE;
    }

    // Send "ls" command.
    if (!ssh->send("ls\n", channel1))
    {
        reportError("ls", ssh->errors());
        ssh->close(channel1);
        return EXIT_FAILURE;
    }

    // Wait for bash prompt, or die in 5 seconds
    if (!ssh->waitFor(channel1, "$", 5))
    {
        reportError("Wait for ls", ssh->errors());
        ssh->close(channel1);
        return EXIT_FAILURE;
    }

    // Fetch recieved data.
    result = ssh->read(channel1);

    if (!result)
    {
        reportError("Read", ssh->errors());
    }
    else
    {
        std::cout << "Received data:" << std::endl << result << std::endl;
    }

    // Terminate connection by sending "exit" command.
    ssh->send("exit\n", channel1);

    return EXIT_SUCCESS;
}

