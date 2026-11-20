#include "model.h"

namespace portalis {
static std::string bindForms(std::string html, const std::string& ticket) {
    if (!validTicket(ticket)) return html;
    // Only this generated script is allowed by the response's CSP nonce.
    html += "<script nonce=\"" + ticket + "\">(() => {const ticket='" + ticket + "';";
    html += R"JS(
let busy = false;
document.addEventListener('submit', async event => {
    event.preventDefault();
    if (busy) return;
    const form = event.target;
    busy = true;
    let status = document.getElementById('portalis-submit-status');
    if (!status) {
        status = document.createElement('p');
        status.id = 'portalis-submit-status';
        status.setAttribute('role', 'status');
        form.appendChild(status);
    }
    status.textContent = 'Saving…';
    try {
        const data = new FormData(form, event.submitter || undefined);
        const fields = [];
        let fileBytes = 0;
        for (const [name, value] of data) {
            if (fields.length >= 64 || new TextEncoder().encode(name).length > 256)
                throw new Error('Maximum 64 fields; field names must be at most 256 bytes.');
            if (value instanceof File) {
                fileBytes += value.size;
                if (fileBytes > 8192) throw new Error('Files must total at most 8 KB.');
                const encoded = await new Promise((resolve, reject) => {
                    const reader = new FileReader();
                    reader.onload = () => resolve(reader.result.split(',')[1] || '');
                    reader.onerror = () => reject(new Error('Could not read the selected file.'));
                    reader.readAsDataURL(value);
                });
                fields.push({name, value: encoded, filename: value.name, type: value.type, encoding: 'base64'});
            } else fields.push({name, value});
        }
        const body = JSON.stringify({ticket, fields});
        if (new TextEncoder().encode(body).length > 16384) throw new Error('Submission exceeds 16 KB. Reduce the input or file size.');
        const response = await fetch('/confirm', {method: 'POST', headers: {'Content-Type': 'application/json'}, body});
        if (!response.ok) throw new Error(await response.text());
        window.location.assign('/done');
    } catch (error) {
        status.textContent = error.message || 'Could not save. Try again.';
        busy = false;
    }
}, true);
})();</script>)JS";
    return html;
}
std::string renderPage(const PageTemplate& requested, const std::string& ticket, bool done) {
    if (done) return R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Connected</title><style>
*{box-sizing:border-box}body{margin:0;min-height:100vh;min-height:100svh;display:grid;place-items:center;padding:24px;background:#fff;color:#202124;font:16px/1.5 system-ui,sans-serif;text-align:center}
main{max-width:320px}svg{display:block;width:72px;height:72px;margin:0 auto 24px}h1{margin:0;font-size:24px;font-weight:500}p{margin:12px 0 0;color:#5f6368}
</style></head><body><main>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" fill="none" stroke="currentColor" stroke-width="4" stroke-linecap="round" aria-hidden="true">
<path d="M8 23a36 36 0 0 1 48 0M16 33a24 24 0 0 1 32 0M24 43a12 12 0 0 1 16 0"/>
<circle cx="32" cy="52" r="3" fill="currentColor" stroke="none"/>
</svg><h1>Connected</h1><p>You can close this page.</p>
</main></body></html>)HTML";
    const PageTemplate page = validTemplate(requested) ? requested : PageTemplate{};
    if (!page.html.empty()) return bindForms(page.html, ticket);
    std::string out = R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Portalis connection demo</title><style>
*{box-sizing:border-box}body{margin:0;min-height:100svh;display:grid;place-items:center;padding:28px;font:18px/1.55 system-ui,sans-serif;background:var(--bg);color:#fff}main{width:min(100%,480px)}.brand{font-size:14px;font-weight:700;letter-spacing:.22em;color:var(--accent)}.orb{height:100px;width:100px;border:2px solid var(--accent);border-radius:50%;margin:38px 0;position:relative;box-shadow:0 0 60px color-mix(in srgb,var(--accent) 20%,transparent)}.orb:before{content:"";position:absolute;inset:15px;border:1px solid var(--accent);border-radius:50%;animation:breathe 3s ease-in-out infinite}.orb:after{content:"";position:absolute;inset:38px;border-radius:50%;background:var(--accent)}h1{font-size:clamp(32px,8vw,46px);line-height:1.12;letter-spacing:-.04em;overflow-wrap:anywhere}p{opacity:.85;overflow-wrap:anywhere}button{width:100%;border:0;border-radius:16px;padding:20px 16px;font:700 18px/1.3 system-ui;background:var(--accent);color:#111;cursor:pointer}small{display:block;font-size:14px;opacity:.7;margin-top:24px}a{color:var(--accent)}@keyframes breathe{50%{transform:scale(.8);opacity:.4}}@media(prefers-reduced-motion:reduce){.orb:before{animation:none}}
</style></head><body style="--accent:)HTML";
    out += page.accent + ";--bg:" + page.background + "\"><main><div class=\"brand\">PORTALIS / CONNECTION DEMO</div><div class=\"orb\" aria-hidden=\"true\"></div>";
    out += "<h1>" + escapeHtml(page.title) + "</h1><p>" + escapeHtml(page.message) + "</p>";
    if (validTicket(ticket)) {
        out += "<form method=\"post\" action=\"/confirm\"><button type=\"submit\">I connected successfully</button></form>";
    } else {
        out += "<p>Please reload the page to get a fresh confirmation button.</p>";
    }
    out += "<small>This is a local connection demo, not an internet login. No account details are requested. Submitted form fields are stored locally on the SD card.</small></main></body></html>";
    return bindForms(std::move(out), ticket);
}
} // namespace portalis
