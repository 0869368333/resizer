gem install specific_install
gem specific_install https://github.com/nathanaeljones/copyright-header

rbenv rehash

run from root of repository:

    copyright-header --license-file ./docs/apache-header.txt --syntax ./docs/license-syntax.yml -o . -a Core:Plugins/DiagnosticJson:Plugins/TinyCache:Plugins/Logging:Plugins/Imazen.Profiling:Tools/Builder:Tools/BuildTools:Tools/COMInstaller:Tools/FakeBuilder:Samples:Tests:Plugins/Shared:Plugins/Security

