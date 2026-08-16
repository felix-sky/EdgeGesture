#include "ObsidianParser.h"
#include <QRegularExpression>
#include <QTextStream>
#include <QFileInfo>

bool ObsidianParser::isImageExtension(const QString &path) {
    static const QRegularExpression imgExt(
        QStringLiteral("\\.(png|jpg|jpeg|gif|bmp|svg|webp|ico|tiff?)$"),
        QRegularExpression::CaseInsensitiveOption);
    return path.contains(imgExt);
}

ObsidianLink ObsidianParser::parseLink(const QString &rawLink) {
    ObsidianLink link;
    link.raw = rawLink.trimmed();

    QString content = link.raw;
    if (content.startsWith(QLatin1Char('!'))) {
        link.isEmbed = true;
        content = content.mid(1).trimmed();
    }

    if (content.startsWith(QStringLiteral("[[")) && content.endsWith(QStringLiteral("]]"))) {
        content = content.mid(2, content.length() - 4).trimmed();
    }

    // Split on pipe for alias/dimension: [[target|alias]]
    QString targetAnchorPart = content;
    int pipeIdx = content.indexOf(QLatin1Char('|'));
    if (pipeIdx != -1) {
        targetAnchorPart = content.left(pipeIdx).trimmed();
        link.alias = content.mid(pipeIdx + 1).trimmed();
    }

    // Split target and anchor on '#': [[target#heading]] or [[target#^block-id]]
    int hashIdx = targetAnchorPart.indexOf(QLatin1Char('#'));
    if (hashIdx != -1) {
        link.target = targetAnchorPart.left(hashIdx).trimmed();
        QString anchor = targetAnchorPart.mid(hashIdx + 1).trimmed();
        if (anchor.startsWith(QLatin1Char('^'))) {
            link.blockId = anchor.mid(1).trimmed();
        } else {
            link.heading = anchor;
        }
    } else {
        link.target = targetAnchorPart;
    }

    // Classify as image
    link.isImage = isImageExtension(link.target);

    // Parse image dimensions from alias if applicable
    if (link.isImage && !link.alias.isEmpty()) {
        static const QRegularExpression dimRegex(QStringLiteral("^(\\d+)(?:x(\\d+))?$"));
        QRegularExpressionMatch dimMatch = dimRegex.match(link.alias);
        if (dimMatch.hasMatch()) {
            link.imageWidth = dimMatch.captured(1).toInt();
            if (!dimMatch.captured(2).isEmpty()) {
                link.imageHeight = dimMatch.captured(2).toInt();
            }
        }
    }

    // Compute display text
    if (!link.alias.isEmpty() && link.imageWidth == 0) {
        link.displayText = link.alias;
    } else if (!link.target.isEmpty()) {
        if (!link.heading.isEmpty()) {
            link.displayText = link.target + QStringLiteral(" › ") + link.heading;
        } else if (!link.blockId.isEmpty()) {
            link.displayText = link.target + QStringLiteral(" › ") + link.blockId;
        } else {
            link.displayText = link.target;
        }
    } else {
        if (!link.heading.isEmpty()) {
            link.displayText = link.heading;
        } else if (!link.blockId.isEmpty()) {
            link.displayText = link.blockId + QStringLiteral(" (block)");
        }
    }

    return link;
}

QVector<ObsidianLink> ObsidianParser::extractAllLinks(const QString &text) {
    QVector<ObsidianLink> links;
    static const QRegularExpression linkRegex(QStringLiteral("!?\\[\\[([^\\]]+)\\]\\]"));
    QRegularExpressionMatchIterator it = linkRegex.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        links.append(parseLink(match.captured(0)));
    }
    return links;
}

FrontmatterData ObsidianParser::splitFrontmatter(const QString &fileContent) {
    FrontmatterData data;
    data.color = QStringLiteral("#624a73"); // Default EdgeGesture note color

    // Check if content starts with YAML frontmatter delimiter "---"
    // May have leading BOM or whitespace
    QString trimmedStart = fileContent;
    int startIndex = 0;
    if (trimmedStart.startsWith(QChar(0xFEFF))) { // Strip UTF-8 BOM
        trimmedStart = trimmedStart.mid(1);
        startIndex = 1;
    }

    if (trimmedStart.startsWith(QStringLiteral("---"))) {
        // Find closing "---" on its own line
        static const QRegularExpression closingRegex(QStringLiteral("\r?\n---\r?\n|\r?\n---$"));
        QRegularExpressionMatch match = closingRegex.match(trimmedStart, 3);
        if (match.hasMatch()) {
            int endIndexInTrimmed = match.capturedEnd();
            int endOfFm = startIndex + endIndexInTrimmed;

            data.hasFrontmatter = true;
            data.rawFrontmatter = fileContent.left(endOfFm);
            data.rawBody = fileContent.mid(endOfFm);

            QString innerFm = trimmedStart.mid(3, match.capturedStart() - 3);

            // Parse color (prefer edgegesture-color, fallback color)
            static const QRegularExpression egColorRegex(QStringLiteral("(?:edgegesture-color|color):\\s*([\"']?#[a-fA-F0-9]+[\"']?)"));
            QRegularExpressionMatch colorMatch = egColorRegex.match(innerFm);
            if (colorMatch.hasMatch()) {
                QString c = colorMatch.captured(1).trimmed();
                c.remove(QLatin1Char('"'));
                c.remove(QLatin1Char('\''));
                data.color = c;
            }

            // Parse pinned (prefer edgegesture-pinned, fallback pinned)
            static const QRegularExpression egPinnedRegex(QStringLiteral("(?:edgegesture-pinned|pinned):\\s*(true|false)"),
                                                         QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch pinnedMatch = egPinnedRegex.match(innerFm);
            if (pinnedMatch.hasMatch()) {
                data.isPinned = (pinnedMatch.captured(1).toLower() == QStringLiteral("true"));
            }

            // Parse tags
            // 1. JSON-style: tags: [a, b]
            static const QRegularExpression jsonTagsRegex(QStringLiteral("tags:\\s*\\[([^\\]]*)\\]"));
            QRegularExpressionMatch jsonTagsMatch = jsonTagsRegex.match(innerFm);
            if (jsonTagsMatch.hasMatch()) {
                const QStringList items = jsonTagsMatch.captured(1).split(QLatin1Char(','));
                for (const QString &item : items) {
                    QString tag = item.trimmed();
                    tag.remove(QLatin1Char('"'));
                    tag.remove(QLatin1Char('\''));
                    if (!tag.isEmpty() && !data.tags.contains(tag)) {
                        data.tags.append(tag);
                    }
                }
            } else {
                // 2. YAML list style: tags:\n  - a\n  - b
                static const QRegularExpression yamlTagsRegex(QStringLiteral("^tags:\\s*(?:\\r?\\n)((?:[ \\t]*-[ \\t]*[^\\r\\n]+\\r?\\n?)+)"),
                                                             QRegularExpression::MultilineOption);
                QRegularExpressionMatch yamlTagsMatch = yamlTagsRegex.match(innerFm);
                if (yamlTagsMatch.hasMatch()) {
                    QStringList lines = yamlTagsMatch.captured(1).split(QRegularExpression(QStringLiteral("\r?\n")));
                    for (const QString &line : lines) {
                        QString trimmed = line.trimmed();
                        if (trimmed.startsWith(QLatin1Char('-'))) {
                            QString tag = trimmed.mid(1).trimmed();
                            tag.remove(QLatin1Char('"'));
                            tag.remove(QLatin1Char('\''));
                            if (!tag.isEmpty() && !data.tags.contains(tag)) {
                                data.tags.append(tag);
                            }
                        }
                    }
                }
            }

            // Parse aliases
            static const QRegularExpression jsonAliasesRegex(QStringLiteral("aliases:\\s*\\[([^\\]]*)\\]"));
            QRegularExpressionMatch jsonAliasesMatch = jsonAliasesRegex.match(innerFm);
            if (jsonAliasesMatch.hasMatch()) {
                const QStringList items = jsonAliasesMatch.captured(1).split(QLatin1Char(','));
                for (const QString &item : items) {
                    QString alias = item.trimmed();
                    alias.remove(QLatin1Char('"'));
                    alias.remove(QLatin1Char('\''));
                    if (!alias.isEmpty() && !data.aliases.contains(alias)) {
                        data.aliases.append(alias);
                    }
                }
            } else {
                static const QRegularExpression yamlAliasesRegex(QStringLiteral("^aliases:\\s*(?:\\r?\\n)((?:[ \\t]*-[ \\t]*[^\\r\\n]+\\r?\\n?)+)"),
                                                               QRegularExpression::MultilineOption);
                QRegularExpressionMatch yamlAliasesMatch = yamlAliasesRegex.match(innerFm);
                if (yamlAliasesMatch.hasMatch()) {
                    QStringList lines = yamlAliasesMatch.captured(1).split(QRegularExpression(QStringLiteral("\r?\n")));
                    for (const QString &line : lines) {
                        QString trimmed = line.trimmed();
                        if (trimmed.startsWith(QLatin1Char('-'))) {
                            QString alias = trimmed.mid(1).trimmed();
                            alias.remove(QLatin1Char('"'));
                            alias.remove(QLatin1Char('\''));
                            if (!alias.isEmpty() && !data.aliases.contains(alias)) {
                                data.aliases.append(alias);
                            }
                        }
                    }
                }
            }

            return data;
        }
    }

    data.hasFrontmatter = false;
    data.rawFrontmatter = QString();
    data.rawBody = fileContent;
    return data;
}

QString ObsidianParser::mergeFrontmatter(const QString &originalContent,
                                       const QVariantMap &updatedFields,
                                       const QString &newBody) {
    FrontmatterData data = splitFrontmatter(originalContent);
    QString bodyToUse = (newBody.isNull() ? data.rawBody : newBody);

    if (data.hasFrontmatter) {
        QString fm = data.rawFrontmatter;

        // Detect line ending (\r\n or \n)
        QString newline = fm.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");

        for (auto it = updatedFields.constBegin(); it != updatedFields.constEnd(); ++it) {
            QString key = it.key();
            QVariant val = it.value();

            QString valStr;
            if (val.typeId() == QMetaType::Bool) {
                valStr = val.toBool() ? QStringLiteral("true") : QStringLiteral("false");
            } else if (val.typeId() == QMetaType::QStringList) {
                QStringList list = val.toStringList();
                valStr = QStringLiteral("[") + list.join(QStringLiteral(", ")) + QStringLiteral("]");
            } else {
                valStr = val.toString();
            }

            // Check if key already exists (also check alias keys like color vs edgegesture-color)
            QString searchKeyPattern = QRegularExpression::escape(key);
            if (key == QStringLiteral("edgegesture-color") || key == QStringLiteral("color")) {
                searchKeyPattern = QStringLiteral("(?:edgegesture-color|color)");
            } else if (key == QStringLiteral("edgegesture-pinned") || key == QStringLiteral("pinned")) {
                searchKeyPattern = QStringLiteral("(?:edgegesture-pinned|pinned)");
            }

            QRegularExpression keyRegex(QStringLiteral("(?m)^") + searchKeyPattern + QStringLiteral(":[^\\r\\n]*"));
            if (keyRegex.match(fm).hasMatch()) {
                fm.replace(keyRegex, key + QStringLiteral(": ") + valStr);
            } else {
                // Insert before the last "---"
                int lastMarkerIdx = fm.lastIndexOf(QStringLiteral("---"));
                if (lastMarkerIdx > 0) {
                    QString insertion = key + QStringLiteral(": ") + valStr + newline;
                    fm.insert(lastMarkerIdx, insertion);
                }
            }
        }

        return fm + bodyToUse;
    } else {
        // No frontmatter originally
        if (updatedFields.isEmpty()) {
            return bodyToUse;
        }

        QString newline = QStringLiteral("\n");
        QString fm = QStringLiteral("---") + newline;
        for (auto it = updatedFields.constBegin(); it != updatedFields.constEnd(); ++it) {
            QString key = it.key();
            QVariant val = it.value();
            QString valStr;
            if (val.typeId() == QMetaType::Bool) {
                valStr = val.toBool() ? QStringLiteral("true") : QStringLiteral("false");
            } else if (val.typeId() == QMetaType::QStringList) {
                QStringList list = val.toStringList();
                valStr = QStringLiteral("[") + list.join(QStringLiteral(", ")) + QStringLiteral("]");
            } else {
                valStr = val.toString();
            }
            fm += key + QStringLiteral(": ") + valStr + newline;
        }
        fm += QStringLiteral("---") + newline;

        return fm + bodyToUse;
    }
}

bool ObsidianParser::parseCallout(const QString &quoteText, CalloutInfo &info) {
    info.raw = quoteText;
    QStringList rawLines = quoteText.split(QRegularExpression(QStringLiteral("\r?\n")));
    if (rawLines.isEmpty()) {
        return false;
    }

    // First line must be callout definition: > [!type]+ Title or > [!type]- Title or > [!type] Title
    QString firstLine = rawLines[0].trimmed();
    if (firstLine.startsWith(QLatin1Char('>'))) {
        firstLine = firstLine.mid(1).trimmed();
    }

    static const QRegularExpression calloutHeaderRegex(
        QStringLiteral("^\\[!([a-zA-Z0-9_-]+)\\]([+-]?)(?:[ \\t]+(.*))?$"));
    QRegularExpressionMatch match = calloutHeaderRegex.match(firstLine);
    if (!match.hasMatch()) {
        return false;
    }

    info.type = match.captured(1).toLower();
    info.foldState = match.captured(2);
    info.isFoldable = (info.foldState == QStringLiteral("+") || info.foldState == QStringLiteral("-"));
    info.isCollapsed = (info.foldState == QStringLiteral("-"));

    QString parsedTitle = match.captured(3).trimmed();
    if (!parsedTitle.isEmpty()) {
        info.title = parsedTitle;
    } else {
        // Default title to capitalized type name
        if (!info.type.isEmpty()) {
            info.title = info.type.at(0).toUpper() + info.type.mid(1);
        }
    }

    // Extract remaining body lines (stripping leading '>')
    QStringList bodyLines;
    for (int i = 1; i < rawLines.size(); ++i) {
        QString line = rawLines[i];
        QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1Char('>'))) {
            // Remove the leading '>' and at most one subsequent space
            int gtIdx = line.indexOf(QLatin1Char('>'));
            QString afterGt = line.mid(gtIdx + 1);
            if (afterGt.startsWith(QLatin1Char(' '))) {
                afterGt = afterGt.mid(1);
            }
            bodyLines.append(afterGt);
        } else {
            bodyLines.append(line);
        }
    }

    info.body = bodyLines.join(QLatin1Char('\n')).trimmed();
    return true;
}
