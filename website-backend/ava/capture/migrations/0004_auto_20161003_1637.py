# -*- coding: utf-8 -*-
# Generated by Django 1.10.1 on 2016-10-03 20:37
from __future__ import unicode_literals

from django.db import migrations


class Migration(migrations.Migration):

    dependencies = [
        ('capture', '0003_globalconfiguration'),
    ]

    operations = [
        migrations.AlterModelOptions(
            name='capturenode',
            options={'permissions': (('view_nodes', 'Can see list of nodes and cameras'),)},
        ),
    ]
