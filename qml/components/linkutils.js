// Helpers for plain text that has no facets (profile bios): linkify
// URLs, @handles and #hashtags client-side, and classify tapped links
// for in-app routing.

function escapeHtml(text) {
    return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
}

function linkify(text) {
    if (!text)
        return ""
    var esc = escapeHtml(text)
    // Handles need a dot (like the composer's mention detection);
    // hashtags must start with a letter
    var re = /(https?:\/\/[^\s<]+)|(^|[\s(])@((?:[A-Za-z0-9][A-Za-z0-9-]*\.)+[A-Za-z]{2,})|(^|[\s(])#([A-Za-z][A-Za-z0-9_]*)/g
    var html = esc.replace(re, function (m, url, pre1, handle, pre2, tag) {
        if (url) {
            // Keep trailing punctuation out of the link
            var trail = ""
            var stripped = url.replace(/[.,;:!?)'"]+$/, function (t) {
                trail = t
                return ""
            })
            return '<a href="' + stripped + '">' + stripped + '</a>' + trail
        }
        if (handle)
            return pre1 + '<a href="https://bsky.app/profile/' + handle + '">@'
                    + handle + '</a>'
        if (tag)
            return pre2 + '<a href="https://bsky.app/hashtag/' + tag + '">#'
                    + tag + '</a>'
        return m
    })
    return html.replace(/\n/g, "<br>")
}

// Returns {type: "profile"|"hashtag"|"external", value}
function parseLink(link) {
    var m = link.match(/^https:\/\/bsky\.app\/profile\/([^\/?#]+)\/?$/)
    if (m)
        return { type: "profile", value: decodeURIComponent(m[1]) }
    m = link.match(/^https:\/\/bsky\.app\/hashtag\/([^\/?#]+)/)
    if (m)
        return { type: "hashtag", value: decodeURIComponent(m[1]) }
    return { type: "external", value: link }
}
