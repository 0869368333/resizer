﻿using ImageResizer.Util;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Web;

namespace ImageResizer.Configuration.Performance
{

    class Utilities
    {
        public static string Sha256hex(string input)
        {
            var hash = System.Security.Cryptography.SHA256.Create().ComputeHash(UTF8Encoding.UTF8.GetBytes(input));
            return BitConverter.ToString(hash, 0, 4).Replace("-", "").ToLowerInvariant();
        }

        public static string Sha256Base64(string input)
        {
            var hash = System.Security.Cryptography.SHA256.Create().ComputeHash(UTF8Encoding.UTF8.GetBytes(input));
            return PathUtils.ToBase64U(hash);
        }

        public static string Sha256TruncatedBase64(string input, int bytes)
        {
            var hash = System.Security.Cryptography.SHA256.Create().ComputeHash(UTF8Encoding.UTF8.GetBytes(input));
            return PathUtils.ToBase64U(hash.Take(bytes).ToArray());
        }

        public static long InterlockedMax(ref long location1, long other)
        {
            long copy;
            long finalOriginal;
            {
                copy = Interlocked.Read(ref location1);
                if (copy >= other) return copy;
                finalOriginal = Interlocked.CompareExchange(ref location1, other, copy);
            } while (finalOriginal != copy) ;
            return finalOriginal;
        }
        public static long InterlockedMin(ref long location1, long other)
        {
            long copy;
            long finalOriginal;
            {
                copy = Interlocked.Read(ref location1);
                if (copy <= other) return copy;
                finalOriginal = Interlocked.CompareExchange(ref location1, other, copy);
            } while (finalOriginal != copy) ;
            return finalOriginal;
        }

    }

    internal static class BoolExtensions
    {
        public static string ToShortString(this bool b)
        {
            return b ? "1" : "0";
        }

    }
    internal static class StringExtensions
    {
        /// <summary>
        /// Only lowercases A..Z -> a..z, and only if req.d.
        /// </summary>
        /// <param name="s"></param>
        /// <returns></returns>
        public static string ToLowerOrdinal(this string s)
        {
            StringBuilder b = null;
            for (var i = 0; i < s.Length; i++)
            {
                var c = s[i];
                if (c >= 'A' && c <= 'Z')
                {
                    if (b == null) b = new StringBuilder(s);
                    b[i] = (char)(c + 0x20);
                }
            }
            return b?.ToString() ?? s;
        }
    }

    internal static class AssemblyExtensions
    {
        public static T GetFirstAttribute<T>(this Assembly a)
        {
            try
            {
                object[] attrs = a.GetCustomAttributes(typeof(T), false);
                if (attrs != null && attrs.Length > 0) return (T)attrs[0];
            }
            catch { }
            return default(T);
        }

        public static string GetShortCommit(this Assembly a)
        {
            return string.Concat(GetFirstAttribute<CommitAttribute>(a)?.Value.Take(7));

        }
        public static string GetInformationalVersion(this Assembly a)
        {
            return GetFirstAttribute<AssemblyInformationalVersionAttribute>(a)?.InformationalVersion;
        }
        public static string GetFileVersion(this Assembly a)
        {
            return GetFirstAttribute<AssemblyFileVersionAttribute>(a)?.Version;
        }
    }


    static class PercentileExtensions
    {
        public static long GetPercentile(this long[] data, float percentile)
        {
            if (data.Length == 0)
            {
                return 0;
            }
            float index = Math.Max(0, percentile * data.Length + 0.5f);

            return (data[(int)Math.Max(0, Math.Ceiling(index - 1.5))] +
                    data[(int)Math.Min(Math.Ceiling(index - 0.5), data.Length - 1)]) / 2;


        }

    }

}
