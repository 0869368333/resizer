﻿using ImageResizer.Util;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ImageResizer.Configuration.Performance
{
    public static class InfoAccumuatorExtensions
    {

        public static void Add(this IInfoAccumulator a, string key, Guid value)
        {
            a.AddString(key, PathUtils.ToBase64U(value.ToByteArray()));
        }

        public static void Add(this IInfoAccumulator a, IEnumerable<KeyValuePair<string, string>> items)
        {
            foreach (var pair in items)
            {
                a.AddString(pair.Key, pair.Value);
            }
        }

        public static void Add(this IInfoAccumulator a, string key, bool? value)
        {
            a.AddString(key, value?.ToShortString());
        }

        public static void Add(this IInfoAccumulator a, string key, long? value)
        {
            a.AddString(key, value?.ToString());
        }

        public static void Add(this IInfoAccumulator a, string key, string value)
        {
            a.AddString(key, value);
        }
        public static string ToQueryString(this IInfoAccumulator a, int characterLimit)
        {
            var pairs = a.GetInfo().Where(pair => pair.Value != null && pair.Key != null)
                     .Select(pair => Uri.EscapeDataString(pair.Key) + "=" + Uri.EscapeDataString(pair.Value));
            var sb = new StringBuilder(1000);
            sb.Append("?");
            foreach (var s in pairs)
            {
                if (sb.Length + s.Length + 1 > characterLimit)
                {
                    return sb.ToString();
                }
                else
                {
                    if (sb[sb.Length - 1] != '?') sb.Append("&");
                    sb.Append(s);
                }
            }
            return sb.ToString();
        }


    }
}