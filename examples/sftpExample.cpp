/* An example of ne7ssh library usage. Please change the values in connectWithPassword
   function before compiling.

   If you are testing this with later openssh versions, make sure to add this
   option to your server's configuration file to enable password authentication:

   PasswordAuthentication yes
*/

#include <ne7ssh.h>
#include <iostream>
#include <stdio.h>
#include <string>

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
    FILE* testFi;
    Ne7SftpSubsystem _sftp;
    const char* dirList;

    std::cout << argv[0] << " " << ne7ssh::getVersion() << std::endl;

    if (argc != 4)
    {
        std::cerr << "Error: Three arguments required: " << argv[0] << " <hostname> <username> <password>" << std::endl;
        return EXIT_FAILURE;
    }

    ne7ssh::create();

    // Set SSH connection options.
    ne7ssh::setOptions("aes256-cbc", "hmac-md5");

    // Initiate connection without starting a remote shell.
    channel1 = ne7ssh::connectWithPassword(argv[1], 22, argv[2], argv[3], 0, 20);
    if (channel1 < 0)
    {
        reportError("Connection", ne7ssh::errors());
        return EXIT_FAILURE;
    }

    // Initiate SFTP subsystem.
    if (!ne7ssh::initSftp(_sftp, channel1))
    {
        reportError("Command", ne7ssh::errors());
        return EXIT_FAILURE;
    }

    // Set a timeout for all SFTP communications.
    _sftp.setTimeout(30);

    // Check remote file permissions.
    Ne7SftpSubsystem::fileAttrs attrs;
    if (_sftp.getFileAttrs(attrs, "test.bin", true))
    {
        std::cout << "Permissions: " << std::oct << (attrs.permissions & 0777) << std::endl;
    }

    // Create a local file.
    testFi = fopen("test.bin", "wb+");
    if (!testFi)
    {
        reportError("Open", ne7ssh::errors());
        return EXIT_FAILURE;
    }

    // Download a file.
    if (!_sftp.get("test.bin", testFi))
    {
        reportError("Get", ne7ssh::errors());
        return EXIT_FAILURE;
    }

    // Change directory.
    if (!_sftp.cd("testing"))
    {
        reportError("cd", ne7ssh::errors());
        return EXIT_FAILURE;
    }

    // Upload the file.
    if (!_sftp.put(testFi, "test2.bin"))
    {
        reportError("put", ne7ssh::errors());
        return EXIT_FAILURE;
    }

    // Create a new directory.
    if (!_sftp.mkdir("testing3"))
    {
        reportError("mkdir", ne7ssh::errors());
        return EXIT_FAILURE;
    }

    // Get listing.
    dirList = _sftp.ls(".", true);
    if (!dirList)
    {
        reportError("ls", ne7ssh::errors());
        return EXIT_FAILURE;
    }
    else
    {
        std::cout << "Directory Listing:" << std::endl << dirList << std::endl;
    }

    // Change permisions on newly uploaded file.
    if (!_sftp.chmod("test2.bin", "755"))
    {
        reportError("chmod", ne7ssh::errors());
        return EXIT_FAILURE;
    }
    ne7ssh::destroy();
    return EXIT_SUCCESS;
}

