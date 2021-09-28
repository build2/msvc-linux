// file      : msvc-common/msvc-filter.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <sys/time.h>   // timeval
#include <sys/select.h>

#include <ios>           // ios_base::failure
#include <string>
#include <cstring>       // strchr()
#include <ostream>
#include <utility>       // move()
#include <iostream>
#include <algorithm>     // max()
#include <system_error>
#include <unordered_map>

#include <libbutl/path.hxx>     // path::traits::realize()
#include <libbutl/utility.hxx>  // alpha(), throw_*_error()
#include <libbutl/process.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/fdstream.hxx>

#include <msvc-common/version.hxx>

using namespace std;
using namespace butl;

// Cached mapping of Windows paths to the corresponding POSIX paths.
//
static unordered_map<string, string> path_cache;

inline static bool
path_separator (char c)
{
  return c == '\\' || c == '/';
}

// Replace absolute Windows paths encountered in the line with their POSIX
// representation. Write the result followed by a newline to the stream. Throw
// ostream::failure on IO failures, system_error on others.
//
static void
filter (const char* s, size_t n, ostream& os)
{
  // Strip the terminating 0xOD character if present.
  //
  if (n > 0 && s[n - 1] == '\r')
    --n;

  // Translate absolute Windows paths back to POSIX. The hard part here is to
  // determing the end of the path. For example, the error location has the
  // 'X:\...\foo(10):' form. However, we cannot assume that '(' ends the path;
  // remember 'Program Files (x86)'.
  //
  // To sidestep this whole mess we are going to use this trick: instead of
  // translating the whole path we will only translate its directory part,
  // that is, the longest part that still ends with the directory
  // separator. We will also still recognize ':' and '\'' as path terminators
  // as well as space if it is the first character in the component.
  //
  // We also pass the path through realpath in order to get the actual path
  // rather than Wine's dosdevices links.
  //
  const char* b (s);
  const char* e (s + n);

  for (;;)
  {
    const char* p (b);

    // Line tail should be at least 3 characters long to contain an absolute
    // Windows path.
    //
    bool no_path (e - b < 3);

    if (!no_path)
    {
      // An absolute path must begin with [A-Za-z]:[\/] (like C:\).
      //
      const char* pe (e - 3);

      for (; p != pe; ++p)
      {
        if (alpha (p[0]) && p[1] == ':' && path_separator (p[2]))
          break;
      }

      no_path = p == pe;
    }

    // Bail out if we reached the end of the line with no path found.
    //
    if (no_path)
    {
      os.write (b, e - b);
      os.put ('\n');
      break;
    }

    os.write (b, p - b); // Print characters that preceed the path.

    b = p;                  // Beginnig of the path.
    const char* pe (p + 3); // End of the last path component.

    for (p = pe; p != e; ++p)
    {
      char c (*p);
      if (c == ':' || c == '\'' || (p == pe && c == ' '))
        break;

      if (path_separator (c))
        pe = p + 1;
    }

    // Convert the Windows directory path to POSIX. First check if the mapping
    // is already cached.
    //
    string d (b, pe - b);
    auto i (path_cache.find (d));

    b = pe;

    if (i == path_cache.end ())
    {
      const char* args[] = {"winepath", "-u", d.c_str (), nullptr};

      // Redirect stderr to /dev/null not to mess the output (read more in
      // main()).
      //
      process pr (args, 0, -1, -2);
      ifdstream is (move (pr.in_ofd));

      string pd;
      getline (is, pd);
      is.close ();

      // It's unknown what can cause winepath to fail. At least not a
      // non-existent path. Anyway will consider it fatal.
      //
      if (!pr.wait ())
        throw_generic_error (ECHILD);

      try
      {
        path::traits_type::realize (pd);

        assert (!pd.empty ());

        // Restore the trailing slash.
        //
        if (pd.back () != '/')
          pd += '/';

      }
      catch (const invalid_path&)
      {
        // The path doesn't exist. Let's keep it as provided by winepath.
      }

      i = path_cache.emplace (move (d), move (pd)).first;
    }

    os.write (i->second.c_str (), i->second.size ());
  }
}

int
main (int argc, char* argv[])
try
{
  auto print_usage = [argv]()
  {
    cerr << "usage: " << argv[0]
         << " <diag> <wine-path> <program-path> [arguments]" << endl;
  };

  if (argc < 2)
  {
    cerr << "error: diag stream file descriptor expected" << endl;
    print_usage ();
    return 1;
  }

  string d (argv[1]);
  if (d != "1" && d != "2")
  {
    cerr << "error: invalid diag stream file descriptor" << endl;
    print_usage ();
    return 1;
  }

  size_t diag (stoi (d));

  if (argc < 3)
  {
    cerr << "error: wine path expected" << endl;
    print_usage ();
    return 1;
  }

  if (argc < 4)
  {
    cerr << "error: program path expected" << endl;
    print_usage ();
    return 1;
  }

  // After arguments are validated we will not be printing error messages on
  // failures not to mess the filtered output of the child Windows process.
  // Note that in the case of a failure the text from STDERR can be treated by
  // the calling process as a build tool diagnostics so our message most
  // likelly would be misinterpreted.
  //
  // The reason we still print error message on the arguments parsing failure
  // is that it is likely to be a program misuse rather than runtime error.
  //

  // If diag is 1 then both stdout and stderr of the child process are read and
  // filtered (achieved by redirecting stdout to stderr). Otherwise the data
  // read from child stdout is proxied to own stdout (sounds redundant but
  // prevents Windows process from writing to /dev/null directly which is known
  // to be super slow). The filtered data is written to the diag file
  // descriptor.
  //
  process pr (const_cast <const char**> (&argv[2]), 0, diag == 1 ? 2 : -1, -1);

  // Stream to filter from.
  //
  ifdstream isf (move (pr.in_efd), fdstream_mode::non_blocking);

  // Stream to proxy from.
  //
  ifdstream isp (
    diag == 1 ? nullfd : move (pr.in_ofd), fdstream_mode::non_blocking);

  ostream& osf (diag == 1 ? cout : cerr);     // Stream to filter to.
  ostream* osp (diag == 1 ? nullptr : &cout); // Stream to proxy to.

  // The presense of proxy input and output streams must be correlated.
  //
  assert (isp.is_open () == (osp != nullptr));

  // Will be using ostream::write() solely, so badbit is the only one which
  // needs to be set.
  //
  osf.exceptions (ostream::badbit);

  if (osp != nullptr)
    osp->exceptions (ostream::badbit);

  const size_t nbuf (8192);
  char buf[nbuf + 1]; // Reserve one extra for terminating '\0'.

  bool terminated (false); // Required for Wine bug workaround (see below).
  string line; // Incomplete line.

  while (isf.is_open () || isp.is_open ())
  {
    // Use timeout to workaround the wineserver bug: sometimes the file
    // descriptor that corresponds to the redirected STDERR of a Windows
    // process is not closed when that process is terminated. So if STDERR is
    // redirected to a pipe (as in our case) the reading peer does not receive
    // EOF and hangs forever. We will consider the no-data 100 ms period for an
    // exited process to represent such a situation.
    //
    // Note that it is wineserver who owns the corresponding file descriptor
    // not a wine process which runs the Windows program.
    //
    // Note also that some implementations of select() can modify the timeout
    // value so it is essential to reinitialize it prior to every select()
    // call.
    //
    timeval timeout {0, 100000};

    fd_set rd;
    FD_ZERO (&rd);

    if (isf.is_open ())
      FD_SET (isf.fd (), &rd);

    if (isp.is_open ())
      FD_SET (isp.fd (), &rd);

    int r (select (max (isf.fd (), isp.fd ()) + 1,
                   &rd,
                   nullptr,
                   nullptr,
                   &timeout));

    if (r == -1)
    {
      if (errno == EINTR)
        continue;

      throw_system_error (errno);
    }

    // Timeout occured. Apply wineserver bug workaround if required.
    //
    butl::optional<bool> status;
    if (r == 0 && (status = pr.try_wait ()))
    {
      if (!*status)
        // Handle the child failure outside the loop.
        //
        break;

      // Presumably end of the data reached.
      //
      if (!terminated)
      {
        // We don't know when the process exited. It possibly wasn't writing
        // to the output for quite a long time before terminating a nanosecond
        // ago. But let's wait for another timeout to be sure that the process
        // has terminated a long (enough) time ago.
        //
        terminated = true;
        continue;
      }

      break;
    }

    // Proxy the data if requested.
    //
    if (isp.is_open () && FD_ISSET (isp.fd (), &rd))
    {
      for (;;)
      {
        // The only leagal way to read from non-blocking ifdstream.
        //
        streamsize n (isp.readsome (buf, nbuf));

        if (isp.eof ())
        {
          // End of the data to be proxied reached.
          //
          isp.close ();
          break;
        }

        if (n == 0)
          break; // No data available, try later.

        assert (osp != nullptr);
        osp->write (buf, n);
      }
    }

    // Read & filter.
    //
    if (isf.is_open () && FD_ISSET (isf.fd (), &rd))
    {
      for (;;)
      {
        // The only leagal way to read from non-blocking ifdstream.
        //
        streamsize n (isf.readsome (buf, nbuf));

        if (isf.eof ())
        {
          // End of the data to be filtered reached.
          //
          isf.close ();
          break;
        }

        if (n == 0)
          break; // No data available, try later.

        // Filter buffer content line by line. Concatenate the line with an
        // incomplete one if produced on the previous iteration. Save the last
        // line if incomplete (not terminated with '\n').
        //
        buf[n] = '\0';
        const char* b (buf);

        for (;;)
        {
          const char* le (strchr (b, '\n'));

          if (le == nullptr)
          {
            line += b;
            break;
          }

          if (!line.empty ())
          {
            line.append (b, le - b);
            filter (line.c_str (), line.size (), osf);
            line.clear ();
          }
          else
            filter (b, le - b, osf);

          b = le + 1; // Skip the newline character.
        }
      }
    }
  }

  if (!line.empty ())
    filter (line.c_str (), line.size (), osf);

  isf.close ();
  isp.close ();

  // Passing through the exact child process exit status on failure tends to be
  // a bit hairy as involves handling the situation when the process is
  // terminated with a signal and so exit code is unavailable. Lets implement
  // when really required.
  //
  return pr.wait () ? 0 : 1;
}
catch (const ios_base::failure&)
{
  return 1;
}
// Also handles process_error exception (derived from system_error).
//
catch (const system_error&)
{
  return 1;
}
