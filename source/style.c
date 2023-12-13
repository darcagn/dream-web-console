const char *stylesheet = R"DWC_CSS(
    <style>
    body {
        color: white;
        background-color: black;
        font-size: 12px;
        font-family: "Courier New", sans-serif;
        font-weight: bold;
    }

    a:link {
        color: white;
    }

    a:visited {
        color: white;
    }

    a:hover {
        color: gray;
    }

    a:active {
        color: red;
    }

    td {
        padding: 4px;
    }

    .top {
        color: white;
        background-color: darkblue;
        font-size: 18px;
    }
    </style>
    )DWC_CSS";

const char *html_footer = "\n\t<hr />dream web console <a href=\"dwc-source-" VERSION ".zip\">" VERSION
                          "</a> server - <a href=\"http://dreamcast.wiki/dwc\">https://dreamcast.wiki/dwc</a>\n</body>\n</html>";
