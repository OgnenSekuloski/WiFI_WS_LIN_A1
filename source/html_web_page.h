/******************************************************************************
* File Name: html_web_page.h
*
* Description: This file contains the HTML pages that the server will host and
*              macros required for http transaction.
*
********************************************************************************
 * (c) 2021-2025, Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG. All rights reserved.
 * This software, associated documentation and materials ("Software") is
 * owned by Infineon Technologies AG or one of its affiliates ("Infineon")
 * and is protected by and subject to worldwide patent protection, worldwide
 * copyright laws, and international treaty provisions. Therefore, you may use
 * this Software only as provided in the license agreement accompanying the
 * software package from which you obtained this Software. If no license
 * agreement applies, then any use, reproduction, modification, translation, or
 * compilation of this Software is prohibited without the express written
 * permission of Infineon.
 *
 * Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
 * IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
 * THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
 * SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
 * Infineon reserves the right to make changes to the Software without notice.
 * You are responsible for properly designing, programming, and testing the
 * functionality and safety of your intended application of the Software, as
 * well as complying with any legal requirements related to its use. Infineon
 * does not guarantee that the Software will be free from intrusion, data theft
 * or loss, or other breaches ("Security Breaches"), and Infineon shall have
 * no liability arising out of any Security Breaches. Unless otherwise
 * explicitly approved by Infineon, the Software may not be used in any
 * application where a failure of the Product or any consequences of the use
 * thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Include guard
*******************************************************************************/
#ifndef HTML_WEB_PAGE_H_
#define HTML_WEB_PAGE_H_

/*******************************************************************************
* Macros
******************************************************************************/
/* Company Logo */
#define LOGO \
    "<div class=\"container\"> "\
    "<img alt=\"logo.png\" "\
    "src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAATkAAABcBAMAAADngd+fAA" \
           "AAD1BMVEX///8VWJbiOlVtkLHIxNm177myAAAEAklEQVR4AezBgQAAAACAoP2pF6kCA" \
           "ABmxgxw29VhMO4WHwArHABlOQBpOIAJvv+ZnmPolnbbwn9Dev2EPNJK3m+fHZPRlmRf" \
           "JMKvh7aRwSvKXAsMryg0NnhFdcmKyvCSIupf1DgVErnzjJMkp8KJ0p2WLJJqhtPkR6L" \
           "hPfvfPOxIdSZe9iHS9JGdV/5Lj+xiOEVX7z2851qIkHr4rRbadFqrKNz4sYpOkAY4qD" \
           "XNX1unmk6zDtM7nZLR9Fu6hehU89CGcEc1HcJhurdHOqrFp1gXoKL7sqp80LuObPNHR" \
           "0V9NQHQgsZ6MohmbnYdG10XOxr0xiVIvV6JGGAx2LKjMUp0LbqF9nXcS4tEk+aO/aI3" \
           "EPdA5dcwLORwbhYWNjpSleiAesvRW5OzfQP4ZaOvt4f0kRKY5iIquEpAQJPeKKSuNjr" \
           "LrmGeWoUdKzrWON/pBriQUF8cUS6rVcM7EqlWUrp4VZSZeHErcVkxDSJIcxw6DY3RlS" \
           "u6qSPWfGB0rlyLK2vSq99iw7tHYdn9SJqIYOn1Pk5ArAGQ4OI6/XRoth3c6RgrumFHW" \
           "4Z9qfa1vENhqNT1G91C7hMdd4Xucozu8kHXP9E5pOkrOkxpnW+3Oa1p5R2HXA27FiIQ" \
           "TT5A3Ohwo7PJgJbuD96RaqNzn+lu6e2W0k1/vFUDZVLsaaMnoysf9+pdAbMudPr9Qlz" \
           "2EP8zXX+ULhU85dPr3TvTYMs4CG90qLHes7rEYl6n4U/eORH5wbtk3qW3is7U0YQidz" \
           "ogLnRDAes1p5tA1xYODONvvTNU+IYOtO20qKkInuimZLbvfWd0pSPjFDWzAVsu1NCiG" \
           "3/cs6qDe7aic3C50yFtdD0UusXy2EQZOge6bM278L13eiHf593UnHdIpr2IVDx/ors4" \
           "8647Nu/s/PS9dxcb55FWmo54B7RrMhct21LT9epX9ayQOLQPUOO3dLg/z4jgEF2kTfv" \
           "N9pyt6dC+stCDudtsvHCn657oLDvrmmZoVbY+fA5I1RmlriwSG123obsjR2OGbyWsAe" \
           "Xw+c7ES32++1JoyaEpr4J/kDTPxhOg6az/K0Y4rEczuvRIt9JjtbIwewx5CygIeQyIE" \
           "vLoOaCwHDHvMB6OT5V9Si8fAlXAEHzmMAbPOWD2cMV8zXqFEHL2oxwxz/NBuAA/a42b" \
           "Ukyg8tkrXQjjqDBidNd89Uo3+qx0DE3lw3jY7FBMd7GlDlLolEV8HtU29c5flVKCmoq" \
           "Z4Tw8+1sP4vHZ72SbxhlcG68UVgBOxWvZlw8PHhGBc3X1JvneNx/+75faX79zx/wC75" \
           "SNwRSC4MYlcv/I1i/C96QALyH8ClDgdYTy4Jr81x4cEwAAACAMsn9qM+wHlgEAAADAA" \
           "To83zvHyP+JAAAAAElFTkSuQmCC\" /> " \
           "<div class=\"topleft\"></div> " \
    "</div>"

#define PROVISIONING_THEME_STYLE \
    "<meta charset=\"utf-8\">" \
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">" \
    "<style>" \
        ":root{" \
            "--pv-navy:#13213d;" \
            "--pv-blue:#2855a3;" \
            "--pv-cyan:#19a9c4;" \
            "--pv-surface:#ffffff;" \
            "--pv-border:rgba(19,33,61,0.10);" \
            "--pv-shadow:0 16px 36px rgba(19,33,61,0.10);" \
            "--pv-text:#14213d;" \
            "--pv-muted:#617089;" \
            "--pv-soft:#eef5ff;" \
            "--pv-success:#148173;" \
            "--pv-warn:#c96a18;" \
        "}" \
        "*{box-sizing:border-box;}" \
        "body{" \
            "margin:0;" \
            "min-height:100vh;" \
            "font-family:'Segoe UI Variable','Segoe UI',Tahoma,sans-serif;" \
            "color:var(--pv-text);" \
            "background:" \
                "radial-gradient(circle at 14% 16%, rgba(43,141,255,0.34), transparent 24%)," \
                "radial-gradient(circle at 84% 18%, rgba(40,85,163,0.36), transparent 27%)," \
                "linear-gradient(135deg, #d9e7ff 0%, #c5dafb 48%, #b7d1f3 100%);" \
        "}" \
        ".pv-page{" \
            "min-height:100vh;" \
            "display:flex;" \
            "align-items:center;" \
            "justify-content:center;" \
            "padding:10px;" \
        "}" \
        ".pv-shell{" \
            "width:min(780px,100%);" \
            "background:var(--pv-surface);" \
            "border:1px solid var(--pv-border);" \
            "border-radius:24px;" \
            "box-shadow:var(--pv-shadow);" \
            "overflow:hidden;" \
        "}" \
        ".pv-header{" \
            "display:flex;" \
            "align-items:center;" \
            "justify-content:space-between;" \
            "gap:16px;" \
            "flex-wrap:wrap;" \
            "padding:16px 16px 8px;" \
        "}" \
        ".pv-header .container{position:static;}" \
        ".pv-header img{display:block;width:min(190px,46vw);height:auto;}" \
        ".pv-badge{" \
            "padding:7px 11px;" \
            "border-radius:999px;" \
            "background:#e4eefb;" \
            "color:var(--pv-blue);" \
            "font-weight:700;" \
            "font-size:12px;" \
            "letter-spacing:.02em;" \
        "}" \
        ".pv-content{padding:14px 16px 18px;}" \
        ".pv-hero{" \
            "margin-bottom:14px;" \
            "text-align:center;" \
        "}" \
        ".pv-hero h1{" \
            "margin:0 0 6px;" \
            "font-size:clamp(1.45rem,3.4vw,2.1rem);" \
            "line-height:1.08;" \
        "}" \
        ".pv-hero p{" \
            "margin:0 auto;" \
            "max-width:610px;" \
            "color:var(--pv-muted);" \
            "line-height:1.45;" \
            "font-size:.95rem;" \
        "}" \
        ".pv-card{" \
            "padding:14px;" \
            "border-radius:18px;" \
            "background:rgba(241,247,253,0.86);" \
            "border:1px solid rgba(19,33,61,0.08);" \
            "margin-top:12px;" \
        "}" \
        ".pv-card h2{" \
            "margin:0 0 6px;" \
            "font-size:1rem;" \
        "}" \
        ".pv-card p{" \
            "margin:0 0 10px;" \
            "color:var(--pv-muted);" \
            "line-height:1.42;" \
            "font-size:.92rem;" \
        "}" \
        ".pv-fieldset{margin:0;padding:0;border:0;}" \
        ".pv-grid{display:grid;gap:10px;}" \
        ".pv-actions{display:grid;gap:10px;margin-top:10px;}" \
        ".pv-label{display:block;margin-bottom:5px;font-size:.9rem;font-weight:700;}" \
        ".pv-input," \
        ".pv-list{" \
            "width:100%;" \
            "border:1px solid rgba(19,33,61,0.14);" \
            "border-radius:14px;" \
            "background:#fff;" \
            "color:var(--pv-text);" \
            "font:inherit;" \
        "}" \
        ".pv-input{padding:12px 13px;}" \
        ".pv-input:focus,.pv-button:focus,.pv-link:focus,.pv-list:focus{" \
            "outline:2px solid rgba(25,169,196,0.35);" \
            "outline-offset:2px;" \
        "}" \
        ".pv-list{min-height:170px;padding:12px;resize:vertical;line-height:1.4;}" \
        ".pv-button," \
        ".pv-link{" \
            "display:inline-flex;" \
            "align-items:center;" \
            "justify-content:center;" \
            "width:100%;" \
            "min-height:48px;" \
            "padding:11px 14px;" \
            "border:0;" \
            "border-radius:15px;" \
            "font:inherit;" \
            "font-weight:700;" \
            "text-align:center;" \
            "text-decoration:none;" \
            "cursor:pointer;" \
        "}" \
        ".pv-button-primary{color:#fff;background:linear-gradient(145deg,#214a90 0%,#2f68c3 100%);}" \
        ".pv-button-secondary{color:var(--pv-blue);background:#e8f0fb;}" \
        ".pv-button-dark{color:#fff;background:linear-gradient(145deg,#16213d 0%,#2a3b65 100%);}" \
        ".pv-note{padding:11px 12px;border-radius:14px;background:#fff;border:1px solid rgba(19,33,61,0.08);color:var(--pv-muted);font-size:.9rem;line-height:1.4;}" \
        ".pv-status{display:flex;align-items:center;gap:10px;margin:0 0 6px;font-size:1rem;font-weight:700;}" \
        ".pv-status-dot{width:10px;height:10px;border-radius:50%;background:var(--pv-success);box-shadow:0 0 0 8px rgba(20,129,115,0.12);}" \
        ".pv-status-dot.warn{background:var(--pv-warn);box-shadow:0 0 0 8px rgba(201,106,24,0.12);}" \
        ".pv-inline-code{font-weight:700;word-break:break-all;}" \
        "@media (min-width:700px){" \
            ".pv-grid-2{grid-template-columns:repeat(2,minmax(0,1fr));}" \
            ".pv-actions-2{grid-template-columns:repeat(2,minmax(0,1fr));}" \
        "}" \
        "@media (max-width:640px){" \
            ".pv-page{padding:8px;}" \
            ".pv-shell{border-radius:20px;}" \
            ".pv-header{padding:14px 14px 6px;}" \
            ".pv-content{padding:12px 14px 14px;}" \
        "}" \
    "</style>"

#define PROVISIONING_PAGE_START(page_title, badge_text) \
    "<!DOCTYPE html>" \
    "<html lang=\"en\">" \
    "<head>" \
        "<title>" page_title "</title>" \
        PROVISIONING_THEME_STYLE \
    "</head>" \
    "<body>" \
        "<main class=\"pv-page\">" \
            "<section class=\"pv-shell\">" \
                "<header class=\"pv-header\">" \
                    "<div>" LOGO "</div>" \
                    "<div class=\"pv-badge\">" badge_text "</div>" \
                "</header>" \
                "<section class=\"pv-content\">"

#define PROVISIONING_PAGE_END \
                "</section>" \
            "</section>" \
        "</main>" \
    "</body>" \
    "</html>"

/* Landing page, user input Wi-Fi network and credentials */
#define HTTP_SOFTAP_STARTUP_WEBPAGE \
    PROVISIONING_PAGE_START("PSoC Wi-Fi Provisioning", "Provisioning") \
        "<div class=\"pv-hero\">" \
            "<h1>Connect the device to Wi-Fi</h1>" \
            "<p>Enter Wi-Fi details or scan for nearby networks.</p>" \
        "</div>" \
        "<form method=\"post\" class=\"pv-card\">" \
            "<fieldset class=\"pv-fieldset\">" \
                "<h2>Wi-Fi credentials</h2>" \
                "<label class=\"pv-label\" for=\"ssid_input\">SSID</label>" \
                "<input class=\"pv-input\" id=\"ssid_input\" type=\"text\" placeholder=\"Enter Wi-Fi name\" name=\"SSID\" autocomplete=\"ssid\" />" \
                "<label class=\"pv-label\" for=\"password_input\">Password</label>" \
                "<input class=\"pv-input\" id=\"password_input\" type=\"password\" placeholder=\"Enter password\" name=\"Password\" minlength=\"8\" autocomplete=\"current-password\" />" \
                "<div class=\"pv-actions\">" \
                    "<input class=\"pv-button pv-button-primary\" type=\"submit\" name=\"submit\" value=\"Connect to Wi-Fi\"/>" \
                "</div>" \
            "</fieldset>" \
        "</form>" \
        "<form action=\"/wifi_scan_form\" method=\"get\" class=\"pv-card\">" \
            "<fieldset class=\"pv-fieldset\">" \
                "<h2>Scan nearby networks</h2>" \
                "<div class=\"pv-actions\">" \
                    "<input class=\"pv-button pv-button-secondary\" type=\"submit\" name=\"submit\" value=\"Scan for Wi-Fi Access Points\"/>" \
                "</div>" \
            "</fieldset>" \
        "</form>" \
    PROVISIONING_PAGE_END

/* HTML Page - Indicates scan for available APs is in progress.*/
#define WIFI_SCAN_IN_PROGRESS \
    PROVISIONING_PAGE_START("Scanning Wi-Fi Networks", "Provisioning") \
        "<div class=\"pv-hero\">" \
            "<h1>Scanning for available access points</h1>" \
            "<p>The device is checking for nearby Wi-Fi networks. Please keep this page open for a moment.</p>" \
        "</div>" \
        "<section class=\"pv-card\">" \
            "<div class=\"pv-status\"><span class=\"pv-status-dot\"></span>Scan in progress</div>" \
            "<p>Results will appear automatically when the scan completes.</p>" \
        "</section>" \
    PROVISIONING_PAGE_END

/* HTML Page - Lists available APs along with LogIn option.*/
#define SOFTAP_SCAN_START_RESPONSE \
    PROVISIONING_PAGE_START("Available Wi-Fi Networks", "Provisioning") \
        "<div class=\"pv-hero\">" \
            "<h1>Available Wi-Fi networks</h1>" \
            "<p>Select a name from the list and enter the password.</p>" \
        "</div>" \
        "<section class=\"pv-card\">" \
            "<h2>Scan results</h2>" \
            "<textarea class=\"pv-list\" readonly>"


#define SOFTAP_SCAN_INTERMEDIATE_RESPONSE \
            "</textarea>" \
        "</section>"

#define SOFTAP_SCAN_END_RESPONSE \
    "<form action=\"/\" method=\"post\" class=\"pv-card\">" \
        "<fieldset class=\"pv-fieldset\">" \
            "<h2>Connect</h2>" \
            "<label class=\"pv-label\" for=\"scan_ssid_input\">SSID</label>" \
            "<input class=\"pv-input\" id=\"scan_ssid_input\" type=\"text\" placeholder=\"Enter Wi-Fi name\" name=\"SSID\" autocomplete=\"ssid\" />" \
            "<label class=\"pv-label\" for=\"scan_password_input\">Password</label>" \
            "<input class=\"pv-input\" id=\"scan_password_input\" type=\"password\" placeholder=\"Enter password\" name=\"Password\" minlength=\"8\" autocomplete=\"current-password\" />" \
            "<div class=\"pv-actions\">" \
                "<input class=\"pv-button pv-button-primary\" type=\"submit\" name=\"submit\" value=\"Connect to Wi-Fi\"/>" \
                "<a class=\"pv-link pv-button-secondary\" href=\"/\">Back to home</a>" \
            "</div>" \
        "</fieldset>" \
    "</form>" \
    PROVISIONING_PAGE_END

/* HTML Page - Indicates connecting to AP whose credentials are entered is in progress.*/
#define WIFI_CONNECT_IN_PROGRESS \
    PROVISIONING_PAGE_START("Connecting to Wi-Fi", "Provisioning") \
        "<div class=\"pv-hero\">" \
            "<h1>Connecting to Wi-Fi</h1>" \
            "<p>This temporary network may disappear in a few seconds.</p>" \
        "</div>" \
        "<section class=\"pv-card\">" \
            "<div class=\"pv-status\"><span class=\"pv-status-dot\"></span>Connecting</div>" \
            "<div class=\"pv-note\">1. Connect your phone/PC to your the same Wi-Fi as the PSOC.</div>" \
            "<div class=\"pv-note\" style=\"margin-top:10px;\">2. Open <span class=\"pv-inline-code\">http://psoc-actuator.local/</span></div>" \
            "<div class=\"pv-note\" style=\"margin-top:10px;\">3. If the hostname does not open, use the device IP displayed on the PSOC display, or from your router client list.</div>" \
        "</section>" \
    PROVISIONING_PAGE_END

/* HTML Device Data Page - Data  */
#define SOFTAP_DEVICE_DATA \
        "<!DOCTYPE html>" \
        "<html lang=\"en\">" \
        "<head>" \
            "<meta charset=\"utf-8\">" \
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">" \
            "<title>PSoC LIN Actuator Control</title>" \
            "<style>" \
                ":root {" \
                    "--psoc-navy:#13213d;" \
                    "--psoc-blue:#2855a3;" \
                    "--psoc-cyan:#19a9c4;" \
                    "--psoc-surface:#ffffff;" \
                    "--psoc-border:rgba(19,33,61,0.10);" \
                    "--psoc-shadow:0 18px 40px rgba(19,33,61,0.10);" \
                    "--psoc-text:#14213d;" \
                    "--psoc-muted:#617089;" \
                    "--psoc-success:#148173;" \
                "}" \
                "*{box-sizing:border-box;}" \
                "body{" \
                    "margin:0;" \
                    "min-height:100vh;" \
                    "font-family:'Segoe UI Variable','Segoe UI',Tahoma,sans-serif;" \
                    "color:var(--psoc-text);" \
                    "background:" \
                        "radial-gradient(circle at 12% 18%, rgba(43,141,255,0.42), transparent 26%)," \
                        "radial-gradient(circle at 85% 20%, rgba(40,85,163,0.52), transparent 30%)," \
                        "radial-gradient(circle at 78% 82%, rgba(76,129,230,0.34), transparent 26%)," \
                        "linear-gradient(135deg, #cfe1ff 0%, #bdd5fb 46%, #adc8f4 100%);" \
                "}" \
                ".page{" \
                    "min-height:100vh;" \
                    "display:flex;" \
                    "align-items:center;" \
                    "justify-content:center;" \
                    "padding:10px;" \
                "}" \
                ".shell{" \
                    "width:min(900px,100%);" \
                    "background:var(--psoc-surface);" \
                    "border:1px solid var(--psoc-border);" \
                    "border-radius:24px;" \
                    "box-shadow:var(--psoc-shadow);" \
                "}" \
                ".header{" \
                    "display:flex;" \
                    "align-items:center;" \
                    "justify-content:space-between;" \
                    "gap:16px;" \
                    "flex-wrap:wrap;" \
                    "padding:16px 16px 8px;" \
                "}" \
                ".header .container{position:static;}" \
                ".header img{display:block;width:min(190px,46vw);height:auto;}" \
                ".badge{" \
                    "padding:7px 11px;" \
                    "border-radius:999px;" \
                    "background:#e4eefb;" \
                    "color:var(--psoc-blue);" \
                    "font-weight:600;" \
                    "font-size:12px;" \
                "}" \
                ".content{" \
                    "padding:14px 16px 16px;" \
                "}" \
                ".intro{" \
                    "margin-bottom:12px;" \
                    "text-align:center;" \
                "}" \
                ".intro h1{" \
                    "margin:0 0 6px;" \
                    "font-size:clamp(1.55rem,3.2vw,2.2rem);" \
                    "line-height:1.08;" \
                "}" \
                ".intro p{" \
                    "margin:0;" \
                    "color:var(--psoc-muted);" \
                    "line-height:1.45;" \
                    "max-width:600px;" \
                    "margin-left:auto;" \
                    "margin-right:auto;" \
                    "font-size:0.95rem;" \
                "}" \
                ".actions{" \
                    "display:grid;" \
                    "grid-template-columns:repeat(3, minmax(0,1fr));" \
                    "gap:10px;" \
                "}" \
                ".action-btn{" \
                    "display:flex;" \
                    "flex-direction:column;" \
                    "align-items:stretch;" \
                    "justify-content:center;" \
                    "gap:7px;" \
                    "min-height:92px;" \
                    "width:100%;" \
                    "padding:10px 12px;" \
                    "border-radius:18px;" \
                    "border:1px solid var(--psoc-border);" \
                    "color:#fff;" \
                    "cursor:pointer;" \
                    "text-align:center;" \
                    "transition:transform .2s ease, box-shadow .2s ease, opacity .2s ease;" \
                    "box-shadow:0 8px 18px rgba(19,33,61,0.10);" \
                "}" \
                ".action-btn:hover{transform:translateY(-2px);}" \
                ".action-btn:disabled{opacity:0.68;cursor:not-allowed;transform:none;}" \
                ".action-btn .label{" \
                    "font-size:1rem;" \
                    "font-weight:700;" \
                "}" \
                ".action-btn .meta{" \
                    "display:block;" \
                    "font-size:0.8rem;" \
                    "line-height:1.35;" \
                    "color:rgba(255,255,255,0.86);" \
                    "text-align:left;" \
                "}" \
                ".action-btn .icon{" \
                    "display:grid;" \
                    "place-items:center;" \
                    "width:32px;" \
                    "height:32px;" \
                    "border-radius:10px;" \
                    "background:rgba(255,255,255,0.16);" \
                    "font-size:15px;" \
                    "font-weight:700;" \
                "}" \
                ".action-top{" \
                    "display:flex;" \
                    "align-items:center;" \
                    "gap:9px;" \
                    "text-align:left;" \
                "}" \
                ".calibrate{background:linear-gradient(145deg, #214a90 0%, #2f68c3 100%);}" \
                ".open{background:linear-gradient(145deg, #118896 0%, #19b1c7 100%);}" \
                ".close{background:linear-gradient(145deg, #16213d 0%, #2a3b65 100%);}" \
                ".status-panel{" \
                    "margin-top:12px;" \
                    "padding:14px;" \
                    "border-radius:18px;" \
                    "background:rgba(241,247,253,0.86);" \
                    "border:1px solid rgba(19,33,61,0.08);" \
                "}" \
                ".status-panel h2{" \
                    "margin:0 0 6px;" \
                    "font-size:1rem;" \
                    "display:flex;" \
                    "align-items:center;" \
                    "gap:10px;" \
                "}" \
                ".status-panel p{" \
                    "margin:0 0 10px;" \
                    "color:var(--psoc-muted);" \
                    "line-height:1.4;" \
                    "font-size:0.92rem;" \
                "}" \
                ".status-box{" \
                    "padding:14px;" \
                    "min-height:110px;" \
                    "max-height:160px;" \
                    "border-radius:16px;" \
                    "background:#fff;" \
                    "border:1px solid rgba(19,33,61,0.08);" \
                    "overflow:auto;" \
                "}" \
                ".status-dot{" \
                    "width:10px;" \
                    "height:10px;" \
                    "border-radius:50%;" \
                    "background:var(--psoc-success);" \
                    "box-shadow:0 0 0 8px rgba(18,140,126,0.12);" \
                "}" \
                "#device_data{" \
                    "font-size:0.98rem;" \
                    "line-height:1.65;" \
                    "color:var(--psoc-text);" \
                    "word-break:break-word;" \
                "}" \
                "@media (max-width: 900px){" \
                    ".actions{grid-template-columns:1fr;}" \
                    ".action-btn{min-height:80px;}" \
                "}" \
                "@media (max-width: 640px){" \
                    ".page{padding:8px;}" \
                    ".shell{border-radius:20px;}" \
                    ".header{padding:14px 14px 6px;}" \
                    ".content{padding:12px 14px 14px;}" \
                "}" \
            "</style>" \
        "</head>" \
        "<body>" \
            "<main class=\"page\">" \
                "<section class=\"shell\">" \
                    "<header class=\"header\">" \
                        "<div>" LOGO "</div>" \
                        "<div class=\"badge\">PSoC LIN Control</div>" \
                    "</header>" \
                    "<section class=\"content\">" \
                        "<div class=\"intro\">" \
                            "<h1>LIN actuator control</h1>" \
                            "<p>Send commands to the actuator and monitor live device feedback in one simple responsive control page.</p>" \
                        "</div>" \
                        "<div class=\"actions\">" \
                            "<button type=\"button\" class=\"action-btn calibrate\" onclick=\"sendCommand('CALIBRATE')\" id=\"calibrate_btn\">" \
                                "<div class=\"action-top\"><div class=\"icon\">C</div><div class=\"label\">Calibrate</div></div>" \
                                "<div class=\"meta\">Set the actuator reference position.</div>" \
                            "</button>" \
                            "<button type=\"button\" class=\"action-btn open\" onclick=\"sendCommand('OPEN')\" id=\"open_btn\">" \
                                "<div class=\"action-top\"><div class=\"icon\">O</div><div class=\"label\">Open</div></div>" \
                                "<div class=\"meta\">Move toward the open state.</div>" \
                            "</button>" \
                            "<button type=\"button\" class=\"action-btn close\" onclick=\"sendCommand('CLOSE')\" id=\"close_btn\">" \
                                "<div class=\"action-top\"><div class=\"icon\">X</div><div class=\"label\">Close</div></div>" \
                                "<div class=\"meta\">Move toward the closed state.</div>" \
                            "</button>" \
                        "</div>" \
                        "<div class=\"status-panel\">" \
                            "<h2><span class=\"status-dot\"></span>Live status</h2>" \
                            "<p>Device updates appear here automatically.</p>" \
                            "<div class=\"status-box\">" \
                                "<div id=\"device_data\">Waiting for status from the PSoC actuator controller...</div>" \
                            "</div>" \
                        "</div>" \
                    "</section>" \
                "</section>" \
            "</main>" \
            "<script>" \
                "function setButtonsDisabled(disabled, text) {" \
                    "var calibrate = document.getElementById('calibrate_btn');" \
                    "var open = document.getElementById('open_btn');" \
                    "var close = document.getElementById('close_btn');" \
                    "calibrate.disabled = disabled;" \
                    "open.disabled = disabled;" \
                    "close.disabled = disabled;" \
                    "if (disabled) {" \
                        "calibrate.querySelector('.label').innerText = text;" \
                        "open.querySelector('.label').innerText = text;" \
                        "close.querySelector('.label').innerText = text;" \
                    "} else {" \
                        "calibrate.querySelector('.label').innerText = 'Calibrate';" \
                        "open.querySelector('.label').innerText = 'Open';" \
                        "close.querySelector('.label').innerText = 'Close';" \
                    "}" \
                "}" \
                "function sendCommand(cmd) {" \
                    "setButtonsDisabled(true, 'Working...');" \
                    "var xhttp = new XMLHttpRequest();" \
                    "xhttp.onreadystatechange = function() {" \
                        "if (this.readyState === 4) {" \
                            "setTimeout(function(){ setButtonsDisabled(false, ''); }, 900);" \
                        "}" \
                    "};" \
                    "xhttp.open('POST', '/', true);" \
                    "xhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');" \
                    "xhttp.send(cmd);" \
                "}" \
                "if (typeof(EventSource) !== 'undefined') {" \
                    "var source = new EventSource('/events');" \
                    "source.onmessage = function(event) {" \
                        "document.getElementById('device_data').innerHTML = event.data;" \
                    "};" \
                "} else {" \
                    "document.getElementById('device_data').innerHTML = 'Your browser does not support live status updates.';" \
                "}" \
            "</script>" \
        "</body>" \
        "</html>"

#endif /* HTTP_PAGES_H_ */

/* [] END OF FILE */
