<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="xml" version="1.0" encoding="UTF-8" indent="yes"/>

  <xsl:template match="/job">
    <xsl:copy select=".">
      <xsl:apply-templates select="recipeSet"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="recipeSet">
    <xsl:copy select=".">
      <xsl:apply-templates select="recipe"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="recipe">
    <xsl:copy select=".">
      <xsl:attribute name="whiteboard">
        <xsl:value-of select="@whiteboard"/>
      </xsl:attribute>
      <xsl:attribute name="role">
        <xsl:value-of select="@role"/>
      </xsl:attribute>
      <xsl:apply-templates select="task"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="task">
    <xsl:copy select=".">
      <xsl:attribute name="name">
        <xsl:value-of select="@name"/>
      </xsl:attribute>
      <xsl:attribute name="role">
        <xsl:value-of select="@role"/>
      </xsl:attribute>
      <xsl:apply-templates select="params"/>
      <xsl:apply-templates select="rpm"/>
      <xsl:apply-templates select="fetch"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="params">
    <xsl:copy select=".">
      <xsl:apply-templates select="param"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="param">
    <xsl:copy select=".">
      <xsl:attribute name="name">
        <xsl:value-of select="@name"/>
      </xsl:attribute>
      <xsl:attribute name="value">
        <xsl:value-of select="@value"/>
      </xsl:attribute>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="rpm">
    <xsl:copy select=".">
      <xsl:attribute name="name">
        <xsl:value-of select="@name"/>
      </xsl:attribute>
      <xsl:attribute name="path">
        <xsl:value-of select="@path"/>
      </xsl:attribute>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="fetch">
    <xsl:copy select=".">
      <xsl:attribute name="url">
        <xsl:value-of select="@url"/>
      </xsl:attribute>
    </xsl:copy>
  </xsl:template>

</xsl:stylesheet>
