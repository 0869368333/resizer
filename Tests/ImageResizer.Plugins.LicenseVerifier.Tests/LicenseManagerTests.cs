﻿using System;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using ImageResizer.Configuration;
using Moq;
using Moq.Protected;
using Xunit;
using Xunit.Abstractions;

namespace ImageResizer.Plugins.LicenseVerifier.Tests
{
    public class LicenseManagerTests
    {
        public LicenseManagerTests(ITestOutputHelper output) { this.output = output; }

        readonly ITestOutputHelper output;

        Mock<HttpMessageHandler> MockRemoteLicense(LicenseManagerSingleton mgr, HttpStatusCode code, string value,
                                                   Action<HttpRequestMessage, CancellationToken> callback)
        {
            var handler = new Mock<HttpMessageHandler>();
            var method = handler.Protected()
                                .Setup<Task<HttpResponseMessage>>("SendAsync", ItExpr.IsAny<HttpRequestMessage>(),
                                    ItExpr.IsAny<CancellationToken>())
                                .Returns(Task.Run(() => new HttpResponseMessage(code) {
                                    Content = new StringContent(value, System.Text.Encoding.UTF8)
                                }));

            if (callback != null) {
                method.Callback(callback);
            }

            method.Verifiable("SendAsync must be called");

            mgr.SetHttpMessageHandler(handler.Object, true);
            return handler;
        }

        Mock<HttpMessageHandler> MockRemoteLicenseException(LicenseManagerSingleton mgr, WebExceptionStatus status)
        {
            var ex = new HttpRequestException("Mock failure", new WebException("Mock failure", status));
            var handler = new Mock<HttpMessageHandler>();
            var method = handler.Protected()
                                .Setup<Task<HttpResponseMessage>>("SendAsync", ItExpr.IsAny<HttpRequestMessage>(),
                                    ItExpr.IsAny<CancellationToken>())
                                .ThrowsAsync(ex);

            method.Verifiable("SendAsync must be called");

            mgr.SetHttpMessageHandler(handler.Object, true);
            return handler;
        }

        [Fact]
        public void Test_Caching_With_Timeout()
        {
            if (Environment.GetEnvironmentVariable("APPVEYOR") == "True") {
                return;
            }

            var clock = new RealClock();
            var cache = new StringCacheMem();

            // Populate cache
            {
                var mgr = new LicenseManagerSingleton(ImazenPublicKeys.Test, clock) {
                    Cache = cache
                };
                MockRemoteLicense(mgr, HttpStatusCode.OK, LicenseStrings.EliteSubscriptionRemote,
                    null);

                var conf = new Config();
                conf.Plugins.LicenseScope = LicenseAccess.Local;
                conf.Plugins.Install(new LicensedPlugin(mgr, clock, "R4Elite"));
                conf.Plugins.AddLicense(LicenseStrings.EliteSubscriptionPlaceholder);
                
                mgr.WaitForTasks();

                var result = new Computation(conf, ImazenPublicKeys.Test, mgr, mgr, clock);
                Assert.True(result.LicensedForRequestUrl(new Uri("http://anydomain")));

                Assert.Empty(mgr.GetIssues());
                Assert.NotNull(conf.GetLicensesPage());
            }

            // Use cache
            {
                var mgr = new LicenseManagerSingleton(ImazenPublicKeys.Test, clock) {
                    Cache = cache
                };
                MockRemoteLicenseException(mgr, WebExceptionStatus.NameResolutionFailure);

                var conf = new Config();
                try {
                    conf.Plugins.LicenseScope = LicenseAccess.Local;
                    conf.Plugins.Install(new LicensedPlugin(mgr, clock, "R4Elite"));
                    conf.Plugins.AddLicense(LicenseStrings.EliteSubscriptionPlaceholder);

                    mgr.Heartbeat();
                    mgr.WaitForTasks();

                    var result = new Computation(conf, ImazenPublicKeys.Test, mgr, mgr, clock);
                    Assert.True(result.LicensedForRequestUrl(new Uri("http://anydomain")));


                    Assert.NotNull(conf.GetDiagnosticsPage());
                    Assert.NotNull(conf.GetLicensesPage());

                    Assert.Equal(1, mgr.GetIssues().Count());
                } catch {
                    output.WriteLine(conf.GetDiagnosticsPage());
                    throw;
                }
            }
        }

        [Fact]
        public void Test_GlobalCache()
        {
            // We don't want to test the singleton

            var unique_prefix = "test_cache_" + Guid.NewGuid() + "__";
            var cacheType = Type.GetType("ImageResizer.Plugins.WriteThroughCache, ImageResizer");
            Debug.Assert(cacheType != null, "cacheType != null");
            var cacheCtor = cacheType.GetConstructor(BindingFlags.NonPublic | BindingFlags.Instance, null,
                new[] {typeof(string)}, null);
            var cacheInstance = cacheCtor.Invoke(new object[] {unique_prefix});


            var c = new PeristentGlobalStringCache();
            var cacheField = typeof(PeristentGlobalStringCache)
                .GetField("cache", BindingFlags.NonPublic | BindingFlags.Instance);
            Debug.Assert(cacheField != null, "cacheField != null");
            cacheField.SetValue(c, cacheInstance);

            Assert.Equal(StringCachePutResult.WriteComplete, c.TryPut("a", "b"));
            Assert.Equal(StringCachePutResult.Duplicate, c.TryPut("a", "b"));
            Assert.Equal("b", c.Get("a"));
            Assert.Equal(null, c.Get("404"));
            Assert.Equal(StringCachePutResult.WriteComplete, c.TryPut("a", null));
            Assert.NotNull(Config.Current.GetDiagnosticsPage());
            Assert.NotNull(Config.Current.GetLicensesPage());
        }


        [Fact]
        public void Test_Offline_License_Failure()
        {
            var clock = new RealClock();
            var mgr = new LicenseManagerSingleton(ImazenPublicKeys.Test, clock) {
                Cache = new StringCacheMem()
            };
            var conf = new Config();

            conf.Plugins.LicenseScope = LicenseAccess.Local;
            conf.Plugins.Install(new LicensedPlugin(mgr, clock, "R4Creative"));

            Assert.Empty(mgr.GetIssues());
            Assert.Null(mgr.GetAllLicenses().FirstOrDefault());

            var result = new Computation(conf, ImazenPublicKeys.Test, mgr, mgr, clock);

            Assert.False(result.LicensedForRequestUrl(new Uri("http://acme.com")));
            conf.Plugins.AddLicense(LicenseStrings.Offlinev4DomainAcmeComCreative);

            Assert.NotNull(mgr.GetAllLicenses().First());

            result = new Computation(conf, ImazenPublicKeys.Test, mgr, mgr, clock);
            Assert.True(result.LicensedForRequestUrl(new Uri("http://acme.com")));

            Assert.Empty(mgr.GetIssues());
            Assert.NotNull(conf.GetDiagnosticsPage());
            Assert.NotNull(conf.GetLicensesPage());
        }


        [Fact]
        public void Test_Offline_License_Success()
        {
            var clock = new RealClock();
            var mgr = new LicenseManagerSingleton(ImazenPublicKeys.Test, clock) {
                Cache = new StringCacheMem()
            };
            var conf = new Config(new ResizerSection(
                @"<resizer><licenses>
      <maphost from='localhost' to='acme.com' />
      <license>" + LicenseStrings.Offlinev4DomainAcmeComCreative + "</license></licenses></resizer>"));

            conf.Plugins.LicenseScope = LicenseAccess.Local;
            conf.Plugins.Install(new LicensedPlugin(mgr, clock, "R4Creative"));

            Assert.Equal(0, mgr.WaitForTasks());
            Assert.Empty(mgr.GetIssues());

            Assert.NotNull(mgr.GetAllLicenses().First());

            var result = new Computation(conf, ImazenPublicKeys.Test, mgr, mgr, clock);

            Assert.True(result.LicensedForRequestUrl(new Uri("http://acme.com")));
            Assert.True(result.LicensedForRequestUrl(new Uri("http://subdomain.acme.com")));
            Assert.True(result.LicensedForRequestUrl(new Uri("http://localhost")));
            Assert.False(result.LicensedForRequestUrl(new Uri("http://other.com")));
            Assert.Equal(0, mgr.WaitForTasks());
            Assert.Empty(mgr.GetIssues());
            Assert.NotNull(conf.GetDiagnosticsPage());
            Assert.NotNull(conf.GetLicensesPage());
        }


        [Fact]
        public void Test_Remote_License_Success()
        {
            if (Environment.GetEnvironmentVariable("APPVEYOR") == "True") {
                return;
            }
            var clock = new RealClock();
            var mgr = new LicenseManagerSingleton(ImazenPublicKeys.Test, clock) {
                Cache = new StringCacheMem()
            };
            Uri invokedUri = null;
            var httpHandler = MockRemoteLicense(mgr, HttpStatusCode.OK, LicenseStrings.EliteSubscriptionRemote,
                (r, c) => { invokedUri = r.RequestUri; });
            var conf = new Config();
            try {
                conf.Plugins.LicenseScope = LicenseAccess.Local;
                conf.Plugins.Install(new LicensedPlugin(mgr, clock, "R4Elite"));
                conf.Plugins.AddLicense(LicenseStrings.EliteSubscriptionPlaceholder);

                Assert.Equal(1, mgr.GetAllLicenses().Count());
                Assert.True(mgr.GetAllLicenses().First().IsRemote);
                mgr.Heartbeat();

                mgr.WaitForTasks();
                Assert.Empty(mgr.GetIssues());

                Mock.Verify(httpHandler);
                Assert.StartsWith(
                    "https://s3-us-west-2.amazonaws.com/licenses.imazen.net/v1/licenses/latest/1qggq12t2qwgwg4c2d2dqwfweqfw.txt?",
                    invokedUri.ToString());


                Assert.NotNull(mgr.GetAllLicenses().First().FetchedLicense());

                var result = new Computation(conf, ImazenPublicKeys.Test, mgr, mgr, clock);

                Assert.True(result.LicensedForRequestUrl(new Uri("http://anydomain")));
                Assert.Equal(0, mgr.WaitForTasks());
                Assert.Empty(mgr.GetIssues());
                Assert.NotNull(conf.GetDiagnosticsPage());
                Assert.NotNull(conf.GetLicensesPage());
            } catch {
                output.WriteLine(conf.GetDiagnosticsPage());
                throw;
            }
        }
        //        Cache = new StringCacheEmpty()
        //    {
        //    var mgr = new LicenseManagerSingleton()
        //{
        //public void Test_Uncached_403()


        //[Fact]
        // test mixed

        // Test invalid content

        //Test network grace period
        //    };

        //    var httpHandler = MockRemoteLicense(mgr, HttpStatusCode.Forbidden, "", null);

        //    Config conf = new Config();
        //    conf.Plugins.LicenseScope = LicenseAccess.Local;
        //    conf.Plugins.Install(new LicensedPlugin(mgr, "R4Elite"));
        //    conf.Plugins.AddLicense(LicenseStrings.EliteSubscriptionPlaceholder);

        //    var tasks = mgr.GetAsyncTasksSnapshot().ToArray();
        //    Assert.Equal(1, tasks.Count());
        //    Task.WaitAll(tasks);

        //    mgr.Heartbeat();
        //    Mock.Verify(httpHandler);

        //    var sink = new IssueSink("LicenseManagerTest");
        //    var result = new Computation(conf, PublicKeys.Test, sink, mgr);


        //    //Assert.NotNull(mgr.GetAllLicenses().First().GetFreshRemoteLicense());
        //    Assert.True(result.LicensedForHost("any"));

        //    tasks = mgr.GetAsyncTasksSnapshot().ToArray();
        //    Assert.Equal(0, tasks.Count());
        //    Task.WaitAll(tasks);

        //}
        // Test with cache states - none, 404, valid, expired, and invalid
        // Test with timeout, 403/404, valid, and invalid response


        //var cacheMock = new Mock<IPersistentStringCache>();
        //cacheMock.Setup((c) => c.Get(It.IsAny<string>())).Returns("404").Verifiable("Cache.Get must be called");
        //cacheMock.Setup((c) => c.TryPut(It.IsAny<string>(), It.IsAny<string>())).Returns(StringCachePutResult.WriteFailed).Verifiable("Cache.TryPut must be called");

        //mgr.Cache = cacheMock.Object;
    }
}
