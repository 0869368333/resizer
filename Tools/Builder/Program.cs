// Copyright (c) Imazen LLC.
// No part of this project, including this file, may be copied, modified,
// propagated, or distributed except as permitted in COPYRIGHT.txt.
// Licensed under the Apache License, Version 2.0.
﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.Win32;

namespace ImageResizer.ReleaseBuilder {
    class Program {
        [STAThread]
        static void Main(string[] args) {

            Build b = new Build();
            b.Run();

            Console.WriteLine("Press a key to exit");
            Console.ReadKey();
        }
    }
}
