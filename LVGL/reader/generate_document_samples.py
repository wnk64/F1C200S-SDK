"""Create small reproducible PDF/EPUB fixtures using only the standard library."""
from pathlib import Path
from xml.etree import ElementTree as ET
from zipfile import ZipFile, ZIP_STORED, ZIP_DEFLATED

target = Path(__file__).with_name("document_samples")
target.mkdir(exist_ok=True)
objects = [b"<< /Type /Catalog /Pages 2 0 R >>", b"<< /Type /Pages /Kids [4 0 R 6 0 R 8 0 R] /Count 3 >>",
           b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"]
for page in range(1, 4):
    stream = (f"0.1 {page / 4:.2f} 0.7 rg 30 180 360 90 re f\n"
              f"0 0 0 rg BT /F1 28 Tf 30 520 Td (PDF PAGE {page}) Tj ET\n"
              "BT /F1 16 Tf 30 470 Td (LVGL native PDF rendering test) Tj ET\n"
              "BT /F1 16 Tf 30 430 Td (Previous / Next keeps original page layout.) Tj ET\n").encode("ascii")
    obj_id = len(objects) + 1
    objects += [(f"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 420 600] "
                 f"/Resources << /Font << /F1 3 0 R >> >> /Contents {obj_id + 1} 0 R >>").encode("ascii"),
                f"<< /Length {len(stream)} >>\nstream\n".encode("ascii") + stream + b"endstream"]
pdf = bytearray(b"%PDF-1.4\n")
positions = [0]
for index, data in enumerate(objects, 1):
    positions.append(len(pdf))
    pdf += f"{index} 0 obj\n".encode("ascii") + data + b"\nendobj\n"
xref = len(pdf)
pdf += f"xref\n0 {len(positions)}\n0000000000 65535 f \n".encode("ascii")
for position in positions[1:]:
    pdf += f"{position:010d} 00000 n \n".encode("ascii")
pdf += (f"trailer\n<< /Size {len(positions)} /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n").encode("ascii")
(target / "sample.pdf").write_bytes(pdf)

container_ns = "urn:oasis:names:tc:opendocument:xmlns:container"
container = ET.Element("container", {"version": "1.0", "xmlns": container_ns})
rootfiles = ET.SubElement(container, "rootfiles")
ET.SubElement(rootfiles, "rootfile", {"full-path": "OEBPS/content.opf", "media-type": "application/oebps-package+xml"})
package = ET.Element("package", {"xmlns": "http://www.idpf.org/2007/opf", "version": "2.0", "unique-identifier": "book-id"})
metadata = ET.SubElement(package, "metadata", {"xmlns:dc": "http://purl.org/dc/elements/1.1/"})
ET.SubElement(metadata, "dc:title").text = "中文 EPUB 阅读测试"
ET.SubElement(metadata, "dc:identifier", {"id": "book-id"}).text = "urn:reader:sample"
ET.SubElement(metadata, "dc:language").text = "zh-CN"
manifest = ET.SubElement(package, "manifest")
ET.SubElement(manifest, "item", {"id": "ncx", "href": "toc.ncx", "media-type": "application/x-dtbncx+xml"})
spine = ET.SubElement(package, "spine", {"toc": "ncx"})
ncx = ET.Element("ncx", {"xmlns": "http://www.daisy.org/z3986/2005/ncx/", "version": "2005-1"})
head = ET.SubElement(ncx, "head")
ET.SubElement(head, "meta", {"name": "dtb:uid", "content": "urn:reader:sample"})
ET.SubElement(ET.SubElement(ncx, "docTitle"), "text").text = "中文 EPUB 阅读测试"
nav = ET.SubElement(ncx, "navMap")
chapters = {}
for number in range(1, 4):
    filename = f"chapter{number}.xhtml"
    ET.SubElement(manifest, "item", {"id": f"c{number}", "href": filename, "media-type": "application/xhtml+xml"})
    ET.SubElement(spine, "itemref", {"idref": f"c{number}"})
    point = ET.SubElement(nav, "navPoint", {"id": f"c{number}", "playOrder": str(number)})
    ET.SubElement(ET.SubElement(point, "navLabel"), "text").text = f"第{number}章"
    ET.SubElement(point, "content", {"src": filename})
    html = ET.Element("html", {"xmlns": "http://www.w3.org/1999/xhtml"})
    ET.SubElement(ET.SubElement(html, "head"), "title").text = f"第{number}章"
    body = ET.SubElement(html, "body")
    ET.SubElement(body, "h1").text = f"第{number}章：中文 EPUB"
    for paragraph in range(1, 9):
        ET.SubElement(body, "p").text = (f"第{number}章，第{paragraph}段。这是EPUB排版测试。"
            "中文标点和英文应当正常显示。文字根据屏幕宽度换行，字号切换后重新排版。"
            "Next page / previous page should work without loading the complete book as images.")
    chapters[filename] = ET.tostring(html, encoding="utf-8", xml_declaration=True)
with ZipFile(target / "sample.epub", "w") as archive:
    archive.writestr("mimetype", "application/epub+zip", compress_type=ZIP_STORED)
    for name, element in {"META-INF/container.xml": container, "OEBPS/content.opf": package, "OEBPS/toc.ncx": ncx}.items():
        archive.writestr(name, ET.tostring(element, encoding="utf-8", xml_declaration=True), compress_type=ZIP_DEFLATED)
    for name, data in chapters.items():
        archive.writestr("OEBPS/" + name, data, compress_type=ZIP_DEFLATED)
print(target)
