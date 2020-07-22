# 2.0版文本小说写作助手现状
## 设定目标
1. 基于大纲层次下降管理，辅助写作
	2. 全书大纲
	3. 分卷大纲
	4. 章节大纲
5. 伏笔管理
	6. 指定故事节点下建立伏笔
	7. 指定章节吸附伏笔（伏笔开端）
	8. 指定章节闭合伏笔（伏笔终结）

## 视图划分
1. 项目文档结构树
2. 小说故事结构树
3. 搜索结果呈现
4. 正文编辑视图
5. 卷宗大纲编辑视图
6. 章节大纲编辑视图
7. 全书大纲编辑视图
8. 卷宗内建伏笔吸附情况汇总
9. 卷宗可见伏笔吸附情况汇总
10. 章节可见伏笔吸附情况汇总

# 2.1版本文本小说写作助手
## 设定目标
1. 继承 2.0 版本
2. 基于sqlite数据库

## 表格设定
##### 作品信息表格（novel_basic）
键名| 名称 | 类型 | 备注
---|---|----|----
id | ID标识 | integer | primary key autoincrement
title | 字段名称 | text | not-null
content| 字段内容|text|nullable
comment|备注|text|nullable

##### 信息树结构组织（keys_tree）
键名| 名称 | 类型 | 备注
---|---|----|----
id| ID标识|integer|primary key autoincrement
type|节点类型|integer|（novel:-1，volume：0，chapter：1，storyblock：2，keypoint：3，despline：4）
parent|父节点|integer|foreign key <id>
nindex|索引|integer|
title|节点名称|text|
desp|节点描述|text

##### 文本内容组织（contents_collect）
键名| 名称 | 类型 | 备注
---|---|----|----
id| id标识|integer|primary key autoincrement
chapter_ref| 定向章节关联|integer|foreign key <keys_tree：id>
content|文章内容|text

##### 故事线驻点(points_collect)
键名| 名称 | 类型 | 备注
---|---|----|----
id|id标识|integer|primary key autoincrement
despline_ref| 故事线引用| integer|foreign key <keys_tree：id> notnull
chapter_attached|章节吸附|integer|foreign key<keys_tree：id>
story_attached|故事吸附|integer|foreign key<keys_tree：id>
nindex|索引|integer
close|闭合指示|integer|default 0
title|节点名称|text
desp|节点描述|text
 
 

-----------------------------------------------
# 3.0版文本小说写作助手规划
## 设定目标
1. 基于大纲层次下降管理，辅助写作
	2. 全书大纲
	3. 分卷大纲
	4. 章节大纲
5. 故事线（伏笔、铺垫）管理
	6. 新建故事线
	7. 章节绑定故事线节点
	8. 章节终结故事线
	
## 基础设定
* 故事线的操作
	* 故事节点树操作
		1. 提供故事线标题+整体描述，指定故事节点建立故事线
		2. 指定主故事线并绑定不同故事线建立驻点及其描述，是否终结故事线
	* 章节树操作
		1. 提供故事线标题+整体描述，指定故事节点建立故事线
		2. 指定章节吸附故事线驻点

## 视图划分
1. 项目文档结构树
2. 小说故事结构树
3. 搜索结果呈现
4. 正文编辑视图
5. 卷宗大纲编辑视图
6. 章节大纲编辑视图
7. 全书大纲编辑视图
8. 卷宗内构建故事线（区分悬空线、绑定线，以便检索故事线防止遗漏）
9. 卷宗承接故事线及其绑定情况（）
