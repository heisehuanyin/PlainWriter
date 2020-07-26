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
 
## 视图改进
### 故事线视图
名称|索引|描述|起始卷|吸附章|关联剧情
---|---|---|---|---|---|---
节点名称|节点索引|描述|卷宗名|章节名|剧情名
* 故事线名称显示：
	* DisplayRole + Icon：【DisplayRole】
	* NodeType：{despline：1，attachpoint：2}【UserRole+1】
	* ParentVolumeIndex：第一视图，筛选本卷故事线【UserRole+2】
* 驻点名称显示
	* DisplayRole + Icon：【DisplayRole】
	* NodeType：{despline：1，attachpoint：2}【UserRole+1】
	* AttachedVolumeIndex：第二视图，标识本卷吸附点【UserRole+2】
	* AttachedChapterID：第三视图，标识本章吸附点【UserRole+3】
* 索引
	* DisplayRole：【DisplayRole】
	* itemID：【UserRole+1】
	
### 视图分类
* 本卷起始支线
* 本卷可见支线【在本卷内有吸附驻点闭合支线+继承的未闭合支线】
* 章节可见支线【继承的未闭合支线+本卷内吸附驻点的支线】

## 操作改进
### 支线操作
* 新增支线和删除支线——只能删除无任何驻点悬空支线
	* 章卷管理界面新建支线
		* 输入支线名称和描述，建立支线
	* 故事结构界面新建支线
		* 输入支线名称和描述，建立支线
	* 支线管理界面新建悬空支线
		* 输入支线名称和描述，建立支线
* 新增驻点和删除驻点——只能删除无任何情节和章节吸附的悬空驻点
	* 支线管理界面附增驻点、插入驻点
		* 输入驻点名称和描述，建立悬空驻点
		* 调整驻点索引，在时间顺序上重排驻点
	* 故事结构界面新增驻点
		* 选择剧情结构，计算合适位置【若位置在章吸附点前，人工确认是否继续】
		* 输入驻点名称和描述，插入驻点
		* 根据指定剧情节点，填充关联剧情字段
* 驻点关联管理
	* 章卷管理界面关联和解除关联
		* 选择支线名称
			* 在悬空列表中选择驻点
			* 是否闭合

-----------------------------------------------
# 3.0版文本小说写作助手规划
## 设定目标
1. 基于大纲层次下降管理，辅助写作
	2. 全书大纲
	3. 分卷大纲
	4. 章节大纲
5. 故事线（伏笔、铺垫）管理
	6. 多故事线管理
	7. 分剧情节点与故事线脱壳
	7. 分卷分阶段指定主线
	8. 故事线内含时间顺序定位
