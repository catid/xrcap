using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnityPlugin
{
    class Program
    {
        static void Main(string[] args)
        {
            string server_address = "localhost";
            int server_port = Xrcap.XRCAP_RENDEZVOUS_PORT;
            string session_name = "Test";
            string password = "password";
            Xrcap.xrcap_connect(server_address, server_port, session_name, password);

            Xrcap.XrcapFrame frame = new Xrcap.XrcapFrame();
            Xrcap.XrcapStatus status = new Xrcap.XrcapStatus();

            int LastFrame = 0;

            while (true)
            {
                Xrcap.xrcap_get(ref frame, ref status);

                Console.WriteLine("State:{0} Mode:{1} CaptureStatus:{2} CameraCount:{3} Mbps:{4}",
                    Xrcap.StateToString(status.State),
                    Xrcap.ModeToString(status.Mode),
                    Xrcap.CaptureStatusToString(status.CaptureStatus),
                    status.CameraCount,
                    status.BitsPerSecond / 1000000.0
                );

                if (frame.Valid != 0 && frame.FrameNumber != LastFrame)
                {
                    LastFrame = frame.FrameNumber;

                    for (int i = 0; i < Xrcap.XRCAP_PERSPECTIVE_COUNT; ++i)
                    {
                        if (frame.Perspectives[i].Valid != 0)
                        {
                            Console.WriteLine("Frame#{0} Perspective {1} : Indices={2} VertexFloats={3}",
                                frame.FrameNumber,
                                i,
                                frame.Perspectives[i].IndicesCount,
                                frame.Perspectives[i].FloatsCount
                            );
                        }
                    }
                }

                Thread.Sleep(10);
            }

            Xrcap.xrcap_shutdown();

            Console.WriteLine("Press ENTER key to terminate");
            Console.ReadLine();
            Console.WriteLine("...Key press detected.  Terminating..");
        }
    }
}
