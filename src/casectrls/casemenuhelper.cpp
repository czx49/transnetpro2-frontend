#include "casemenuhelper.h"

namespace {

LayerItem layerItemFromMenuFields(const QString& name,
                                  const QString& wfsTypeName,
                                  const QString& val,
                                  int level)
{
    LayerItem li;
    li.name = name.trimmed();
    li.wfsTypeName = wfsTypeName.trimmed();
    li.val = val.trimmed();
    li.level = level;
    li.category = layerCategoryFromApiVal(li.val, li.wfsTypeName);
    li.checked = false;
    return li;
}

bool isWfsLayerLeaf(const QString& wfsTypeName, const QString& val)
{
    return !wfsTypeName.trimmed().isEmpty() || !val.trimmed().isEmpty();
}

QList<LayerItem> parseLayerItemsFromChildrenArray(const QJsonArray& childrenArray)
{
    QList<LayerItem> layers;
    for (const QJsonValue& childValue : childrenArray) {
        const QJsonObject childObj = childValue.toObject();
        if (childObj.isEmpty()) {
            continue;
        }
        const LayerItem li = layerItemFromMenuFields(childObj.value(QStringLiteral("name")).toString(),
                                                    childObj.value(QStringLiteral("value")).toString(),
                                                    childObj.value(QStringLiteral("val")).toString(),
                                                    childObj.value(QStringLiteral("level")).toInt());
        if (li.name.isEmpty() || !isWfsLayerLeaf(li.wfsTypeName, li.val)) {
            continue;
        }
        layers.append(li);
    }
    return layers;
}

void appendLayersFromMenuChildren(LayerGroupItem& group, const QList<MenuChildrenJson>& children)
{
    for (const MenuChildrenJson& child : children) {
        const LayerItem li = layerItemFromMenuChild(child);
        if (li.name.isEmpty() || !isWfsLayerLeaf(li.wfsTypeName, li.val)) {
            continue;
        }
        group.layers.append(li);
    }
}

} // namespace

QString layerCategoryFromApiVal(const QString& valRaw, const QString& typeNameFallback)
{
    const QString v = valRaw.trimmed().toLower();
    if (!v.isEmpty()) {
        if (v.startsWith(QStringLiteral("road_")) || v.startsWith(QStringLiteral("slow_"))) {
            return QStringLiteral("road");
        }
        if (v.startsWith(QStringLiteral("bus_")) || v.startsWith(QStringLiteral("pt_"))) {
            return QStringLiteral("bus");
        }
        return QStringLiteral("other");
    }
    const QString t = typeNameFallback.trimmed().toLower();
    if (t.contains(QStringLiteral("_road_")) || t.contains(QStringLiteral("slow_road"))) {
        return QStringLiteral("road");
    }
    if (t.contains(QStringLiteral("_bus_")) || t.startsWith(QStringLiteral("pt_"))) {
        return QStringLiteral("bus");
    }
    return QStringLiteral("other");
}

LayerItem layerItemFromMenuChild(const MenuChildrenJson& child)
{
    return layerItemFromMenuFields(child.j_name(), child.j_value(), child.j_val(), child.j_level());
}

QList<LayerGroupItem> parseLayerGroupsFromJsonArray(const QJsonArray& listArray)
{
    QList<LayerGroupItem> groups;
    for (const QJsonValue& groupValue : listArray) {
        const QJsonObject groupObj = groupValue.toObject();
        if (groupObj.isEmpty()) {
            continue;
        }

        LayerGroupItem group;
        const QString groupName = groupObj.value(QStringLiteral("name")).toString().trimmed();
        const QString groupValueKey = groupObj.value(QStringLiteral("value")).toString().trimmed();
        group.name = groupName.isEmpty() ? groupValueKey : groupName;
        group.value = groupValueKey;

        group.layers = parseLayerItemsFromChildrenArray(groupObj.value(QStringLiteral("children")).toArray());

        if (group.layers.isEmpty() && isWfsLayerLeaf(groupObj.value(QStringLiteral("value")).toString(),
                                                     groupObj.value(QStringLiteral("val")).toString())) {
            const LayerItem li = layerItemFromMenuFields(groupObj.value(QStringLiteral("name")).toString(),
                                                        groupObj.value(QStringLiteral("value")).toString(),
                                                        groupObj.value(QStringLiteral("val")).toString(),
                                                        groupObj.value(QStringLiteral("level")).toInt());
            if (!li.name.isEmpty()) {
                group.layers.append(li);
            }
        }

        if (!group.name.isEmpty() || !group.layers.isEmpty()) {
            groups.append(group);
        }
    }
    return groups;
}

QList<LayerGroupItem> parseLayerGroupsFromCaseJson(const CaseJson& raw)
{
    return parseLayerGroupsFromJsonArray(raw.jsonObject().value(QStringLiteral("list")).toArray());
}

QList<LayerGroupItem> parseLayerGroups(const QList<MenuJson>& menus)
{
    QList<LayerGroupItem> groups;
    for (const MenuJson& menu : menus) {
        const QString groupName = menu.j_name().trimmed();
        const QString groupValue = menu.j_value().trimmed();

        LayerGroupItem group;
        group.name = groupName.isEmpty() ? groupValue : groupName;
        group.value = groupValue;

        const QJsonArray childrenArray = menu.jsonObject().value(QStringLiteral("children")).toArray();
        if (!childrenArray.isEmpty()) {
            group.layers = parseLayerItemsFromChildrenArray(childrenArray);
        } else {
            appendLayersFromMenuChildren(group, menu.j_children());
        }

        if (group.layers.isEmpty() && isWfsLayerLeaf(menu.j_value(), menu.j_val())) {
            const LayerItem li = layerItemFromMenuFields(menu.j_name(), menu.j_value(), menu.j_val(), menu.j_level());
            if (!li.name.isEmpty()) {
                group.layers.append(li);
            }
        }

        if (!group.name.isEmpty() || !group.layers.isEmpty()) {
            groups.append(group);
        }
    }
    return groups;
}

QList<LayerItem> flattenLayerGroups(const QList<LayerGroupItem>& groups)
{
    QList<LayerItem> flat;
    for (const LayerGroupItem& group : groups) {
        for (const LayerItem& layer : group.layers) {
            flat.append(layer);
        }
    }
    return flat;
}

QList<BaseDataLayerGroupNodeItem> buildBaseDataLayerGroups(const QList<MenuJson>& menus)
{
    QList<BaseDataLayerGroupNodeItem> result;
    QJsonArray menuArray;
    for (const MenuJson& menu : menus) {
        menuArray.append(menu.jsonObject());
    }
    const QList<LayerGroupItem> groups = parseLayerGroupsFromJsonArray(menuArray);

    for (const LayerGroupItem& group : groups) {
        BaseDataLayerGroupNodeItem groupNode;
        groupNode.displayName = group.name;
        groupNode.groupValue = group.value;

        for (const LayerItem& layer : group.layers) {
            BaseDataLayerNodeItem node;
            node.displayName = layer.name;
            node.checked = layer.checked;
            node.userData.insert(baseDataUserDataKey(BaseDataUserDataField::Value), layer.wfsTypeName);
            node.userData.insert(baseDataUserDataKey(BaseDataUserDataField::Val), layer.val);
            node.userData.insert(baseDataUserDataKey(BaseDataUserDataField::Level), layer.level);
            groupNode.layers.append(node);
        }

        result.append(groupNode);
    }
    return result;
}
